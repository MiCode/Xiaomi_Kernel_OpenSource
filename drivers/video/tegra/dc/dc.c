/*
 * drivers/video/tegra/dc/dc.c
 *
 * Copyright (C) 2010 Google, Inc.
 * Author: Erik Gilling <konkers@android.com>
 *
 * Copyright (c) 2010-2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/backlight.h>
#include <linux/gpio.h>
#include <linux/nvhost.h>
#include <video/tegrafb.h>
#include <drm/drm_fixed.h>
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif

#define CREATE_TRACE_POINTS
#include <trace/events/display.h>

#include <mach/clk.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/mc.h>
#include <linux/nvhost.h>
#include <mach/latency_allowance.h>
#include <mach/iomap.h>

#include "dc_reg.h"
#include "dc_config.h"
#include "dc_priv.h"
#include "dev.h"
#include "nvsd.h"

#define TEGRA_CRC_LATCHED_DELAY		34

#define DC_COM_PIN_OUTPUT_POLARITY1_INIT_VAL	0x01000000
#define DC_COM_PIN_OUTPUT_POLARITY3_INIT_VAL	0x0

static struct fb_videomode tegra_dc_hdmi_fallback_mode = {
	.refresh = 60,
	.xres = 640,
	.yres = 480,
	.pixclock = KHZ2PICOS(25200),
	.hsync_len = 96,	/* h_sync_width */
	.vsync_len = 2,		/* v_sync_width */
	.left_margin = 48,	/* h_back_porch */
	.upper_margin = 33,	/* v_back_porch */
	.right_margin = 16,	/* h_front_porch */
	.lower_margin = 10,	/* v_front_porch */
	.vmode = 0,
	.sync = 0,
};

static struct tegra_dc_mode override_disp_mode[3];

static void _tegra_dc_controller_disable(struct tegra_dc *dc);

struct tegra_dc *tegra_dcs[TEGRA_MAX_DC];

DEFINE_MUTEX(tegra_dc_lock);
DEFINE_MUTEX(shared_lock);

static const struct {
	bool h;
	bool v;
} can_filter[] = {
	/* Window A has no filtering */
	{ false, false },
	/* Window B has both H and V filtering */
	{ true,  true  },
	/* Window C has only H filtering */
	{ false, true  },
};

#ifdef CONFIG_TEGRA_DC_CMU
static struct tegra_dc_cmu default_cmu = {
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
		0x100, 0x0,   0x0,
		0x0,   0x100, 0x0,
		0x0,   0x0,   0x100,
	},
	/* lut2 maps linear space to sRGB*/
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

void tegra_dc_clk_enable(struct tegra_dc *dc)
{
	if (!tegra_is_clk_enabled(dc->clk)) {
		clk_prepare_enable(dc->clk);
		tegra_dvfs_set_rate(dc->clk, dc->mode.pclk);
	}
}

void tegra_dc_clk_disable(struct tegra_dc *dc)
{
	if (tegra_is_clk_enabled(dc->clk)) {
		clk_disable_unprepare(dc->clk);
		tegra_dvfs_set_rate(dc->clk, 0);
	}
}

void tegra_dc_hold_dc_out(struct tegra_dc *dc)
{
	/* extra reference to dc clk */
	clk_prepare_enable(dc->clk);

	if (dc->out_ops->hold)
		dc->out_ops->hold(dc);
}

void tegra_dc_release_dc_out(struct tegra_dc *dc)
{
	if (dc->out_ops->release)
		dc->out_ops->release(dc);

	/* balance extra dc clk reference */
	clk_disable_unprepare(dc->clk);
}

#define DUMP_REG(a) do {			\
	snprintf(buff, sizeof(buff), "%-32s\t%03x\t%08lx\n",  \
		 #a, a, tegra_dc_readl(dc, a));		      \
	print(data, buff);				      \
	} while (0)

static void _dump_regs(struct tegra_dc *dc, void *data,
		       void (* print)(void *data, const char *str))
{
	int i;
	char buff[256];

	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);

	DUMP_REG(DC_CMD_DISPLAY_COMMAND_OPTION0);
	DUMP_REG(DC_CMD_DISPLAY_COMMAND);
	DUMP_REG(DC_CMD_SIGNAL_RAISE);
	DUMP_REG(DC_CMD_INT_STATUS);
	DUMP_REG(DC_CMD_INT_MASK);
	DUMP_REG(DC_CMD_INT_ENABLE);
	DUMP_REG(DC_CMD_INT_TYPE);
	DUMP_REG(DC_CMD_INT_POLARITY);
	DUMP_REG(DC_CMD_SIGNAL_RAISE1);
	DUMP_REG(DC_CMD_SIGNAL_RAISE2);
	DUMP_REG(DC_CMD_SIGNAL_RAISE3);
	DUMP_REG(DC_CMD_STATE_ACCESS);
	DUMP_REG(DC_CMD_STATE_CONTROL);
	DUMP_REG(DC_CMD_DISPLAY_WINDOW_HEADER);
	DUMP_REG(DC_CMD_REG_ACT_CONTROL);

	DUMP_REG(DC_DISP_DISP_SIGNAL_OPTIONS0);
	DUMP_REG(DC_DISP_DISP_SIGNAL_OPTIONS1);
	DUMP_REG(DC_DISP_DISP_WIN_OPTIONS);
	DUMP_REG(DC_DISP_MEM_HIGH_PRIORITY);
	DUMP_REG(DC_DISP_MEM_HIGH_PRIORITY_TIMER);
	DUMP_REG(DC_DISP_DISP_TIMING_OPTIONS);
	DUMP_REG(DC_DISP_REF_TO_SYNC);
	DUMP_REG(DC_DISP_SYNC_WIDTH);
	DUMP_REG(DC_DISP_BACK_PORCH);
	DUMP_REG(DC_DISP_DISP_ACTIVE);
	DUMP_REG(DC_DISP_FRONT_PORCH);
	DUMP_REG(DC_DISP_H_PULSE0_CONTROL);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_A);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_B);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_C);
	DUMP_REG(DC_DISP_H_PULSE0_POSITION_D);
	DUMP_REG(DC_DISP_H_PULSE1_CONTROL);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_A);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_B);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_C);
	DUMP_REG(DC_DISP_H_PULSE1_POSITION_D);
	DUMP_REG(DC_DISP_H_PULSE2_CONTROL);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_A);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_B);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_C);
	DUMP_REG(DC_DISP_H_PULSE2_POSITION_D);
	DUMP_REG(DC_DISP_V_PULSE0_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE0_POSITION_A);
	DUMP_REG(DC_DISP_V_PULSE0_POSITION_B);
	DUMP_REG(DC_DISP_V_PULSE0_POSITION_C);
	DUMP_REG(DC_DISP_V_PULSE1_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE1_POSITION_A);
	DUMP_REG(DC_DISP_V_PULSE1_POSITION_B);
	DUMP_REG(DC_DISP_V_PULSE1_POSITION_C);
	DUMP_REG(DC_DISP_V_PULSE2_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE2_POSITION_A);
	DUMP_REG(DC_DISP_V_PULSE3_CONTROL);
	DUMP_REG(DC_DISP_V_PULSE3_POSITION_A);
	DUMP_REG(DC_DISP_M0_CONTROL);
	DUMP_REG(DC_DISP_M1_CONTROL);
	DUMP_REG(DC_DISP_DI_CONTROL);
	DUMP_REG(DC_DISP_PP_CONTROL);
	DUMP_REG(DC_DISP_PP_SELECT_A);
	DUMP_REG(DC_DISP_PP_SELECT_B);
	DUMP_REG(DC_DISP_PP_SELECT_C);
	DUMP_REG(DC_DISP_PP_SELECT_D);
	DUMP_REG(DC_DISP_DISP_CLOCK_CONTROL);
	DUMP_REG(DC_DISP_DISP_INTERFACE_CONTROL);
	DUMP_REG(DC_DISP_DISP_COLOR_CONTROL);
	DUMP_REG(DC_DISP_SHIFT_CLOCK_OPTIONS);
	DUMP_REG(DC_DISP_DATA_ENABLE_OPTIONS);
	DUMP_REG(DC_DISP_SERIAL_INTERFACE_OPTIONS);
	DUMP_REG(DC_DISP_LCD_SPI_OPTIONS);
	DUMP_REG(DC_DISP_BORDER_COLOR);
	DUMP_REG(DC_DISP_COLOR_KEY0_LOWER);
	DUMP_REG(DC_DISP_COLOR_KEY0_UPPER);
	DUMP_REG(DC_DISP_COLOR_KEY1_LOWER);
	DUMP_REG(DC_DISP_COLOR_KEY1_UPPER);
	DUMP_REG(DC_DISP_CURSOR_FOREGROUND);
	DUMP_REG(DC_DISP_CURSOR_BACKGROUND);
	DUMP_REG(DC_DISP_CURSOR_START_ADDR);
	DUMP_REG(DC_DISP_CURSOR_START_ADDR_NS);
	DUMP_REG(DC_DISP_CURSOR_POSITION);
	DUMP_REG(DC_DISP_CURSOR_POSITION_NS);
	DUMP_REG(DC_DISP_INIT_SEQ_CONTROL);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_A);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_B);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_C);
	DUMP_REG(DC_DISP_SPI_INIT_SEQ_DATA_D);
	DUMP_REG(DC_DISP_DC_MCCIF_FIFOCTRL);
	DUMP_REG(DC_DISP_MCCIF_DISPLAY0A_HYST);
	DUMP_REG(DC_DISP_MCCIF_DISPLAY0B_HYST);
	DUMP_REG(DC_DISP_MCCIF_DISPLAY0C_HYST);
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
	DUMP_REG(DC_DISP_MCCIF_DISPLAY1B_HYST);
#endif
	DUMP_REG(DC_DISP_DAC_CRT_CTRL);
	DUMP_REG(DC_DISP_DISP_MISC_CONTROL);


	for (i = 0; i < 3; i++) {
		print(data, "\n");
		snprintf(buff, sizeof(buff), "WINDOW %c:\n", 'A' + i);
		print(data, buff);

		tegra_dc_writel(dc, WINDOW_A_SELECT << i,
				DC_CMD_DISPLAY_WINDOW_HEADER);
		DUMP_REG(DC_CMD_DISPLAY_WINDOW_HEADER);
		DUMP_REG(DC_WIN_WIN_OPTIONS);
		DUMP_REG(DC_WIN_BYTE_SWAP);
		DUMP_REG(DC_WIN_BUFFER_CONTROL);
		DUMP_REG(DC_WIN_COLOR_DEPTH);
		DUMP_REG(DC_WIN_POSITION);
		DUMP_REG(DC_WIN_SIZE);
		DUMP_REG(DC_WIN_PRESCALED_SIZE);
		DUMP_REG(DC_WIN_H_INITIAL_DDA);
		DUMP_REG(DC_WIN_V_INITIAL_DDA);
		DUMP_REG(DC_WIN_DDA_INCREMENT);
		DUMP_REG(DC_WIN_LINE_STRIDE);
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
		DUMP_REG(DC_WIN_BUF_STRIDE);
		DUMP_REG(DC_WIN_UV_BUF_STRIDE);
#endif
		DUMP_REG(DC_WIN_BLEND_NOKEY);
		DUMP_REG(DC_WIN_BLEND_1WIN);
		DUMP_REG(DC_WIN_BLEND_2WIN_X);
		DUMP_REG(DC_WIN_BLEND_2WIN_Y);
		DUMP_REG(DC_WIN_BLEND_3WIN_XY);
		DUMP_REG(DC_WINBUF_START_ADDR);
		DUMP_REG(DC_WINBUF_START_ADDR_U);
		DUMP_REG(DC_WINBUF_START_ADDR_V);
		DUMP_REG(DC_WINBUF_ADDR_H_OFFSET);
		DUMP_REG(DC_WINBUF_ADDR_V_OFFSET);
		DUMP_REG(DC_WINBUF_UFLOW_STATUS);
		DUMP_REG(DC_WIN_CSC_YOF);
		DUMP_REG(DC_WIN_CSC_KYRGB);
		DUMP_REG(DC_WIN_CSC_KUR);
		DUMP_REG(DC_WIN_CSC_KVR);
		DUMP_REG(DC_WIN_CSC_KUG);
		DUMP_REG(DC_WIN_CSC_KVG);
		DUMP_REG(DC_WIN_CSC_KUB);
		DUMP_REG(DC_WIN_CSC_KVB);
	}

	DUMP_REG(DC_CMD_DISPLAY_POWER_CONTROL);
	DUMP_REG(DC_COM_PIN_OUTPUT_ENABLE2);
	DUMP_REG(DC_COM_PIN_OUTPUT_POLARITY2);
	DUMP_REG(DC_COM_PIN_OUTPUT_DATA2);
	DUMP_REG(DC_COM_PIN_INPUT_ENABLE2);
	DUMP_REG(DC_COM_PIN_OUTPUT_SELECT5);
	DUMP_REG(DC_DISP_DISP_SIGNAL_OPTIONS0);
	DUMP_REG(DC_DISP_M1_CONTROL);
	DUMP_REG(DC_COM_PM1_CONTROL);
	DUMP_REG(DC_COM_PM1_DUTY_CYCLE);
	DUMP_REG(DC_DISP_SD_CONTROL);
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	DUMP_REG(DC_COM_CMU_CSC_KRR);
	DUMP_REG(DC_COM_CMU_CSC_KGR);
	DUMP_REG(DC_COM_CMU_CSC_KBR);
	DUMP_REG(DC_COM_CMU_CSC_KRG);
	DUMP_REG(DC_COM_CMU_CSC_KGG);
	DUMP_REG(DC_COM_CMU_CSC_KBR);
	DUMP_REG(DC_COM_CMU_CSC_KRB);
	DUMP_REG(DC_COM_CMU_CSC_KGB);
	DUMP_REG(DC_COM_CMU_CSC_KBB);
#endif

	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);
}

#undef DUMP_REG

#ifdef DEBUG
static void dump_regs_print(void *data, const char *str)
{
	struct tegra_dc *dc = data;
	dev_dbg(&dc->ndev->dev, "%s", str);
}

static void dump_regs(struct tegra_dc *dc)
{
	_dump_regs(dc, dc, dump_regs_print);
}
#else /* !DEBUG */

static void dump_regs(struct tegra_dc *dc) {}

#endif /* DEBUG */

#ifdef CONFIG_DEBUG_FS

static void dbg_regs_print(void *data, const char *str)
{
	struct seq_file *s = data;

	seq_printf(s, "%s", str);
}

#undef DUMP_REG

static int dbg_dc_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;

	_dump_regs(dc, s, dbg_regs_print);

	return 0;
}


static int dbg_dc_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_show, inode->i_private);
}

static const struct file_operations regs_fops = {
	.open		= dbg_dc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_dc_mode_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;
	struct tegra_dc_mode *m;

	mutex_lock(&dc->lock);
	m = &dc->mode;
	seq_printf(s,
		"pclk: %d\n"
		"h_ref_to_sync: %d\n"
		"v_ref_to_sync: %d\n"
		"h_sync_width: %d\n"
		"v_sync_width: %d\n"
		"h_back_porch: %d\n"
		"v_back_porch: %d\n"
		"h_active: %d\n"
		"v_active: %d\n"
		"h_front_porch: %d\n"
		"v_front_porch: %d\n"
		"stereo_mode: %d\n",
		m->pclk, m->h_ref_to_sync, m->v_ref_to_sync,
		m->h_sync_width, m->v_sync_width,
		m->h_back_porch, m->v_back_porch,
		m->h_active, m->v_active,
		m->h_front_porch, m->v_front_porch,
		m->stereo_mode);
	mutex_unlock(&dc->lock);
	return 0;
}

static int dbg_dc_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_mode_show, inode->i_private);
}

static const struct file_operations mode_fops = {
	.open		= dbg_dc_mode_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int dbg_dc_stats_show(struct seq_file *s, void *unused)
{
	struct tegra_dc *dc = s->private;

	mutex_lock(&dc->lock);
	seq_printf(s,
		"underflows: %llu\n"
		"underflows_a: %llu\n"
		"underflows_b: %llu\n"
		"underflows_c: %llu\n",
		dc->stats.underflows,
		dc->stats.underflows_a,
		dc->stats.underflows_b,
		dc->stats.underflows_c);
	mutex_unlock(&dc->lock);

	return 0;
}

static int dbg_dc_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_dc_stats_show, inode->i_private);
}

static const struct file_operations stats_fops = {
	.open		= dbg_dc_stats_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static void tegra_dc_remove_debugfs(struct tegra_dc *dc)
{
	if (dc->debugdir)
		debugfs_remove_recursive(dc->debugdir);
	dc->debugdir = NULL;
}

static void tegra_dc_create_debugfs(struct tegra_dc *dc)
{
	struct dentry *retval;

	dc->debugdir = debugfs_create_dir(dev_name(&dc->ndev->dev), NULL);
	if (!dc->debugdir)
		goto remove_out;

	retval = debugfs_create_file("regs", S_IRUGO, dc->debugdir, dc,
		&regs_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("mode", S_IRUGO, dc->debugdir, dc,
		&mode_fops);
	if (!retval)
		goto remove_out;

	retval = debugfs_create_file("stats", S_IRUGO, dc->debugdir, dc,
		&stats_fops);
	if (!retval)
		goto remove_out;

	return;
remove_out:
	dev_err(&dc->ndev->dev, "could not create debugfs\n");
	tegra_dc_remove_debugfs(dc);
}

#else /* !CONFIG_DEBUGFS */
static inline void tegra_dc_create_debugfs(struct tegra_dc *dc) { };
static inline void __devexit tegra_dc_remove_debugfs(struct tegra_dc *dc) { };
#endif /* CONFIG_DEBUGFS */

static int tegra_dc_set(struct tegra_dc *dc, int index)
{
	int ret = 0;

	mutex_lock(&tegra_dc_lock);
	if (index >= TEGRA_MAX_DC) {
		ret = -EINVAL;
		goto out;
	}

	if (dc != NULL && tegra_dcs[index] != NULL) {
		ret = -EBUSY;
		goto out;
	}

	tegra_dcs[index] = dc;

out:
	mutex_unlock(&tegra_dc_lock);

	return ret;
}

unsigned int tegra_dc_has_multiple_dc(void)
{
	unsigned int idx;
	unsigned int cnt = 0;
	struct tegra_dc *dc;

	mutex_lock(&tegra_dc_lock);
	for (idx = 0; idx < TEGRA_MAX_DC; idx++)
		cnt += ((dc = tegra_dcs[idx]) != NULL && dc->enabled) ? 1 : 0;
	mutex_unlock(&tegra_dc_lock);

	return (cnt > 1);
}

/* get the stride size of a window.
 * return: stride size in bytes for window win. or 0 if unavailble. */
int tegra_dc_get_stride(struct tegra_dc *dc, unsigned win)
{
	u32 stride;

	if (!dc->enabled)
		return 0;
	BUG_ON(win > DC_N_WINDOWS);
	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);
	tegra_dc_writel(dc, WINDOW_A_SELECT << win,
		DC_CMD_DISPLAY_WINDOW_HEADER);
	stride = tegra_dc_readl(dc, DC_WIN_LINE_STRIDE);
	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);
	return GET_LINE_STRIDE(stride);
}
EXPORT_SYMBOL(tegra_dc_get_stride);

struct tegra_dc *tegra_dc_get_dc(unsigned idx)
{
	if (idx < TEGRA_MAX_DC)
		return tegra_dcs[idx];
	else
		return NULL;
}
EXPORT_SYMBOL(tegra_dc_get_dc);

struct tegra_dc_win *tegra_dc_get_window(struct tegra_dc *dc, unsigned win)
{
	if (win >= dc->n_windows)
		return NULL;

	return &dc->windows[win];
}
EXPORT_SYMBOL(tegra_dc_get_window);

bool tegra_dc_get_connected(struct tegra_dc *dc)
{
	return dc->connected;
}
EXPORT_SYMBOL(tegra_dc_get_connected);

bool tegra_dc_hpd(struct tegra_dc *dc)
{
	int sense;
	int level;
	int hpd;

	if (WARN_ON(!dc || !dc->out))
		return false;

	if (dc->out->hotplug_state != 0) {
		if (dc->out->hotplug_state == 1) /* force on */
			return true;
		if (dc->out->hotplug_state == -1) /* force off */
			return false;
	}
	level = gpio_get_value(dc->out->hotplug_gpio);

	sense = dc->out->flags & TEGRA_DC_OUT_HOTPLUG_MASK;

	hpd = (sense == TEGRA_DC_OUT_HOTPLUG_HIGH && level) ||
		(sense == TEGRA_DC_OUT_HOTPLUG_LOW && !level);

	if (dc->out->hotplug_report)
		dc->out->hotplug_report(hpd);

	return hpd;
}
EXPORT_SYMBOL(tegra_dc_hpd);

static void tegra_dc_set_scaling_filter(struct tegra_dc *dc)
{
	unsigned i;
	unsigned v0 = 128;
	unsigned v1 = 0;

	/* linear horizontal and vertical filters */
	for (i = 0; i < 16; i++) {
		tegra_dc_writel(dc, (v1 << 16) | (v0 << 8),
				DC_WIN_H_FILTER_P(i));

		tegra_dc_writel(dc, v0,
				DC_WIN_V_FILTER_P(i));
		v0 -= 8;
		v1 += 8;
	}
}

#ifdef CONFIG_TEGRA_DC_CMU
static void tegra_dc_cache_cmu(struct tegra_dc_cmu *dst_cmu,
					struct tegra_dc_cmu *src_cmu)
{
	memcpy(dst_cmu, src_cmu, sizeof(struct tegra_dc_cmu));
}

static void tegra_dc_set_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *cmu)
{
	u32 val;
	u32 i;

	for (i = 0; i < 256; i++) {
		val = LUT1_ADDR(i) | LUT1_DATA(cmu->lut1[i]);
		tegra_dc_writel(dc, val, DC_COM_CMU_LUT1);
	}

	tegra_dc_writel(dc, cmu->csc.krr, DC_COM_CMU_CSC_KRR);
	tegra_dc_writel(dc, cmu->csc.kgr, DC_COM_CMU_CSC_KGR);
	tegra_dc_writel(dc, cmu->csc.kbr, DC_COM_CMU_CSC_KBR);
	tegra_dc_writel(dc, cmu->csc.krg, DC_COM_CMU_CSC_KRG);
	tegra_dc_writel(dc, cmu->csc.kgg, DC_COM_CMU_CSC_KGG);
	tegra_dc_writel(dc, cmu->csc.kbg, DC_COM_CMU_CSC_KBG);
	tegra_dc_writel(dc, cmu->csc.krb, DC_COM_CMU_CSC_KRB);
	tegra_dc_writel(dc, cmu->csc.kgb, DC_COM_CMU_CSC_KGB);
	tegra_dc_writel(dc, cmu->csc.kbb, DC_COM_CMU_CSC_KBB);

	for (i = 0; i < 960; i++) {
		val = LUT2_ADDR(i) | LUT1_DATA(cmu->lut2[i]);
		tegra_dc_writel(dc, val, DC_COM_CMU_LUT2);
	}
}

void tegra_dc_get_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *cmu)
{
	u32 val;
	u32 i;
	bool flags;

	val = tegra_dc_readl(dc, DC_DISP_DISP_COLOR_CONTROL);
	if (val & CMU_ENABLE)
		flags = true;

	val &= ~CMU_ENABLE;
	tegra_dc_writel(dc, val, DC_DISP_DISP_COLOR_CONTROL);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	/*TODO: Sync up with frame end */
	mdelay(20);

	for (i = 0; i < 256; i++) {
		val = LUT1_READ_EN | LUT1_READ_ADDR(i);
		tegra_dc_writel(dc, val, DC_COM_CMU_LUT1_READ);
		val = tegra_dc_readl(dc, DC_COM_CMU_LUT1);
		cmu->lut1[i] = LUT1_READ_DATA(val);
	}

	cmu->csc.krr = tegra_dc_readl(dc, DC_COM_CMU_CSC_KRR);
	cmu->csc.kgr = tegra_dc_readl(dc, DC_COM_CMU_CSC_KGR);
	cmu->csc.kbr = tegra_dc_readl(dc, DC_COM_CMU_CSC_KBR);
	cmu->csc.krg = tegra_dc_readl(dc, DC_COM_CMU_CSC_KRG);
	cmu->csc.kgg = tegra_dc_readl(dc, DC_COM_CMU_CSC_KGG);
	cmu->csc.kbg = tegra_dc_readl(dc, DC_COM_CMU_CSC_KBG);
	cmu->csc.krb = tegra_dc_readl(dc, DC_COM_CMU_CSC_KRB);
	cmu->csc.kgb = tegra_dc_readl(dc, DC_COM_CMU_CSC_KGB);
	cmu->csc.kbb = tegra_dc_readl(dc, DC_COM_CMU_CSC_KBB);

	for (i = 0; i < 960; i++) {
		val = LUT2_READ_EN | LUT2_READ_ADDR(i);
		tegra_dc_writel(dc, val, DC_COM_CMU_LUT2_READ);
		val = tegra_dc_readl(dc, DC_COM_CMU_LUT2);
		cmu->lut2[i] = LUT2_READ_DATA(val);
	}
}
EXPORT_SYMBOL(tegra_dc_get_cmu);

int _tegra_dc_update_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *cmu)
{
	u32 val;

	if (dc->pdata->cmu_enable) {
		dc->pdata->flags |= TEGRA_DC_FLAG_CMU_ENABLE;
	} else {
		dc->pdata->flags &= ~TEGRA_DC_FLAG_CMU_ENABLE;
		return 0;
	}

	if (cmu != &dc->cmu) {
		tegra_dc_cache_cmu(&dc->cmu, cmu);

		/* Disable CMU */
		val = tegra_dc_readl(dc, DC_DISP_DISP_COLOR_CONTROL);
		if (val & CMU_ENABLE) {
			val &= ~CMU_ENABLE;
			tegra_dc_writel(dc, val, DC_DISP_DISP_COLOR_CONTROL);
			val = GENERAL_UPDATE;
			tegra_dc_writel(dc, val, DC_CMD_STATE_CONTROL);
			val = GENERAL_ACT_REQ;
			tegra_dc_writel(dc, val, DC_CMD_STATE_CONTROL);
			/*TODO: Sync up with vsync */
			mdelay(20);
		}

		tegra_dc_set_cmu(dc, &dc->cmu);
	}

	return 0;
}

int tegra_dc_update_cmu(struct tegra_dc *dc, struct tegra_dc_cmu *cmu)
{
	int ret;

	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return 0;
	}
	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);

	ret = _tegra_dc_update_cmu(dc, cmu);
	tegra_dc_set_color_control(dc);

	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);

	return ret;
}
EXPORT_SYMBOL(tegra_dc_update_cmu);

void tegra_dc_cmu_enable(struct tegra_dc *dc, bool cmu_enable)
{
	dc->pdata->cmu_enable = cmu_enable;
	if (dc->pdata->cmu)
		tegra_dc_update_cmu(dc, dc->pdata->cmu);
	else
		tegra_dc_update_cmu(dc, &default_cmu);
}
#else
#define tegra_dc_cache_cmu(dst_cmu, src_cmu)
#define tegra_dc_set_cmu(dc, cmu)
#define tegra_dc_update_cmu(dc, cmu)
#endif

/* disable_irq() blocks until handler completes, calling this function while
 * holding dc->lock can deadlock. */
static inline void disable_dc_irq(const struct tegra_dc *dc)
{
	disable_irq(dc->irq);
}

u32 tegra_dc_get_syncpt_id(const struct tegra_dc *dc, int i)
{
	return dc->syncpt[i].id;
}
EXPORT_SYMBOL(tegra_dc_get_syncpt_id);

u32 tegra_dc_incr_syncpt_max(struct tegra_dc *dc, int i)
{
	u32 max;

	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);
	max = nvhost_syncpt_incr_max_ext(dc->ndev,
		dc->syncpt[i].id, ((dc->enabled) ? 1 : 0));
	dc->syncpt[i].max = max;
	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);

	return max;
}

void tegra_dc_incr_syncpt_min(struct tegra_dc *dc, int i, u32 val)
{
	mutex_lock(&dc->lock);
	if (dc->enabled) {
		tegra_dc_io_start(dc);
		tegra_dc_hold_dc_out(dc);
		while (dc->syncpt[i].min < val) {
			dc->syncpt[i].min++;
			nvhost_syncpt_cpu_incr_ext(dc->ndev, dc->syncpt[i].id);
		}
		tegra_dc_release_dc_out(dc);
		tegra_dc_io_end(dc);
	}
	mutex_unlock(&dc->lock);
}

void
tegra_dc_config_pwm(struct tegra_dc *dc, struct tegra_dc_pwm_params *cfg)
{
	unsigned int ctrl;
	unsigned long out_sel;
	unsigned long cmd_state;

	mutex_lock(&dc->lock);
	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return;
	}

	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);

	ctrl = ((cfg->period << PM_PERIOD_SHIFT) |
		(cfg->clk_div << PM_CLK_DIVIDER_SHIFT) |
		cfg->clk_select);

	/* The new value should be effected immediately */
	cmd_state = tegra_dc_readl(dc, DC_CMD_STATE_ACCESS);
	tegra_dc_writel(dc, (cmd_state | (1 << 2)), DC_CMD_STATE_ACCESS);

	switch (cfg->which_pwm) {
	case TEGRA_PWM_PM0:
		/* Select the LM0 on PM0 */
		out_sel = tegra_dc_readl(dc, DC_COM_PIN_OUTPUT_SELECT5);
		out_sel &= ~(7 << 0);
		out_sel |= (3 << 0);
		tegra_dc_writel(dc, out_sel, DC_COM_PIN_OUTPUT_SELECT5);
		tegra_dc_writel(dc, ctrl, DC_COM_PM0_CONTROL);
		tegra_dc_writel(dc, cfg->duty_cycle, DC_COM_PM0_DUTY_CYCLE);
		break;
	case TEGRA_PWM_PM1:
		/* Select the LM1 on PM1 */
		out_sel = tegra_dc_readl(dc, DC_COM_PIN_OUTPUT_SELECT5);
		out_sel &= ~(7 << 4);
		out_sel |= (3 << 4);
		tegra_dc_writel(dc, out_sel, DC_COM_PIN_OUTPUT_SELECT5);
		tegra_dc_writel(dc, ctrl, DC_COM_PM1_CONTROL);
		tegra_dc_writel(dc, cfg->duty_cycle, DC_COM_PM1_DUTY_CYCLE);
		break;
	default:
		dev_err(&dc->ndev->dev, "Error: Need which_pwm\n");
		break;
	}
	tegra_dc_writel(dc, cmd_state, DC_CMD_STATE_ACCESS);
	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);
}
EXPORT_SYMBOL(tegra_dc_config_pwm);

void tegra_dc_set_out_pin_polars(struct tegra_dc *dc,
				const struct tegra_dc_out_pin *pins,
				const unsigned int n_pins)
{
	unsigned int i;

	int name;
	int pol;

	u32 pol1, pol3;

	u32 set1, unset1;
	u32 set3, unset3;

	set1 = set3 = unset1 = unset3 = 0;

	for (i = 0; i < n_pins; i++) {
		name = (pins + i)->name;
		pol  = (pins + i)->pol;

		/* set polarity by name */
		switch (name) {
		case TEGRA_DC_OUT_PIN_DATA_ENABLE:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set3 |= LSPI_OUTPUT_POLARITY_LOW;
			else
				unset3 |= LSPI_OUTPUT_POLARITY_LOW;
			break;
		case TEGRA_DC_OUT_PIN_H_SYNC:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set1 |= LHS_OUTPUT_POLARITY_LOW;
			else
				unset1 |= LHS_OUTPUT_POLARITY_LOW;
			break;
		case TEGRA_DC_OUT_PIN_V_SYNC:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set1 |= LVS_OUTPUT_POLARITY_LOW;
			else
				unset1 |= LVS_OUTPUT_POLARITY_LOW;
			break;
		case TEGRA_DC_OUT_PIN_PIXEL_CLOCK:
			if (pol == TEGRA_DC_OUT_PIN_POL_LOW)
				set1 |= LSC0_OUTPUT_POLARITY_LOW;
			else
				unset1 |= LSC0_OUTPUT_POLARITY_LOW;
			break;
		default:
			printk("Invalid argument in function %s\n",
			       __FUNCTION__);
			break;
		}
	}

	pol1 = DC_COM_PIN_OUTPUT_POLARITY1_INIT_VAL;
	pol3 = DC_COM_PIN_OUTPUT_POLARITY3_INIT_VAL;

	pol1 |= set1;
	pol1 &= ~unset1;

	pol3 |= set3;
	pol3 &= ~unset3;

	tegra_dc_writel(dc, pol1, DC_COM_PIN_OUTPUT_POLARITY1);
	tegra_dc_writel(dc, pol3, DC_COM_PIN_OUTPUT_POLARITY3);
}

static struct tegra_dc_mode *tegra_dc_get_override_mode(struct tegra_dc *dc)
{
	if (dc->out->type == TEGRA_DC_OUT_RGB ||
		dc->out->type == TEGRA_DC_OUT_HDMI ||
		dc->out->type == TEGRA_DC_OUT_DSI)
		return override_disp_mode[dc->out->type].pclk ?
			&override_disp_mode[dc->out->type] : NULL;
	else
		return NULL;
}

static void tegra_dc_set_out(struct tegra_dc *dc, struct tegra_dc_out *out)
{
	struct tegra_dc_mode *mode;

	dc->out = out;
	mode = tegra_dc_get_override_mode(dc);

	if (mode)
		tegra_dc_set_mode(dc, mode);
	else if (out->n_modes > 0)
		tegra_dc_set_mode(dc, &dc->out->modes[0]);

	switch (out->type) {
	case TEGRA_DC_OUT_RGB:
		dc->out_ops = &tegra_dc_rgb_ops;
		break;

	case TEGRA_DC_OUT_HDMI:
		dc->out_ops = &tegra_dc_hdmi_ops;
		break;

	case TEGRA_DC_OUT_DSI:
		dc->out_ops = &tegra_dc_dsi_ops;
		break;

	default:
		dc->out_ops = NULL;
		break;
	}

	if (dc->out_ops && dc->out_ops->init)
		dc->out_ops->init(dc);
}

/* returns on error: -EINVAL
 * on success: TEGRA_DC_OUT_RGB, TEGRA_DC_OUT_HDMI, or TEGRA_DC_OUT_DSI. */
int tegra_dc_get_out(const struct tegra_dc *dc)
{
	if (dc && dc->out)
		return dc->out->type;
	return -EINVAL;
}

unsigned tegra_dc_get_out_height(const struct tegra_dc *dc)
{
	if (dc->out)
		return dc->out->height;
	else
		return 0;
}
EXPORT_SYMBOL(tegra_dc_get_out_height);

unsigned tegra_dc_get_out_width(const struct tegra_dc *dc)
{
	if (dc->out)
		return dc->out->width;
	else
		return 0;
}
EXPORT_SYMBOL(tegra_dc_get_out_width);

unsigned tegra_dc_get_out_max_pixclock(const struct tegra_dc *dc)
{
	if (dc->out && dc->out->max_pixclock)
		return dc->out->max_pixclock;
	else
		return 0;
}
EXPORT_SYMBOL(tegra_dc_get_out_max_pixclock);

void tegra_dc_enable_crc(struct tegra_dc *dc)
{
	u32 val;

	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);

	val = CRC_ALWAYS_ENABLE | CRC_INPUT_DATA_ACTIVE_DATA |
		CRC_ENABLE_ENABLE;
	tegra_dc_writel(dc, val, DC_COM_CRC_CONTROL);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);
	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);
}

void tegra_dc_disable_crc(struct tegra_dc *dc)
{
	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);
	tegra_dc_writel(dc, 0x0, DC_COM_CRC_CONTROL);
	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);
}

u32 tegra_dc_read_checksum_latched(struct tegra_dc *dc)
{
	int crc = 0;

	if (!dc) {
		pr_err("Failed to get dc: NULL parameter.\n");
		goto crc_error;
	}

#ifndef CONFIG_TEGRA_SIMULATION_PLATFORM
	/* TODO: Replace mdelay with code to sync VBlANK, since
	 * DC_COM_CRC_CHECKSUM_LATCHED is available after VBLANK */
	mdelay(TEGRA_CRC_LATCHED_DELAY);
#endif

	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);
	crc = tegra_dc_readl(dc, DC_COM_CRC_CHECKSUM_LATCHED);
	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);
crc_error:
	return crc;
}

static bool tegra_dc_windows_are_dirty(struct tegra_dc *dc)
{
#ifndef CONFIG_TEGRA_SIMULATION_PLATFORM
	u32 val;

	val = tegra_dc_readl(dc, DC_CMD_STATE_CONTROL);
	if (val & (WIN_A_ACT_REQ | WIN_B_ACT_REQ | WIN_C_ACT_REQ))
	    return true;
#endif
	return false;
}

static inline void enable_dc_irq(const struct tegra_dc *dc)
{
#ifndef CONFIG_TEGRA_FPGA_PLATFORM
	enable_irq(dc->irq);
#else
	/* Always disable DC interrupts on FPGA. */
	disable_irq(dc->irq);
#endif
}

void tegra_dc_get_fbvblank(struct tegra_dc *dc, struct fb_vblank *vblank)
{
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		vblank->flags = FB_VBLANK_HAVE_VSYNC;
}

int tegra_dc_wait_for_vsync(struct tegra_dc *dc)
{
	int ret = -ENOTTY;

	if (!(dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) || !dc->enabled)
		return ret;

	/*
	 * Logic is as follows
	 * a) Indicate we need a vblank.
	 * b) Wait for completion to be signalled from isr.
	 * c) Initialize completion for next iteration.
	 */

	tegra_dc_hold_dc_out(dc);
	dc->out->user_needs_vblank = true;

	ret = wait_for_completion_interruptible(&dc->out->user_vblank_comp);
	init_completion(&dc->out->user_vblank_comp);
	tegra_dc_release_dc_out(dc);

	return ret;
}

static void tegra_dc_prism_update_backlight(struct tegra_dc *dc)
{
	/* Do the actual brightness update outside of the mutex dc->lock */
	if (dc->out->sd_settings && !dc->out->sd_settings->bl_device &&
		dc->out->sd_settings->bl_device_name) {
		char *bl_device_name =
			dc->out->sd_settings->bl_device_name;
		dc->out->sd_settings->bl_device =
			get_backlight_device_by_name(bl_device_name);
	}

	if (dc->out->sd_settings && dc->out->sd_settings->bl_device) {
		struct backlight_device *bl = dc->out->sd_settings->bl_device;
		backlight_update_status(bl);
	}
}

static void tegra_dc_vblank(struct work_struct *work)
{
	struct tegra_dc *dc = container_of(work, struct tegra_dc, vblank_work);
	bool nvsd_updated = false;

	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return;
	}

	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);
	/* use the new frame's bandwidth setting instead of max(current, new),
	 * skip this if we're using tegra_dc_one_shot_worker() */
	if (!(dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE))
		tegra_dc_program_bandwidth(dc, true);

	/* Clear the V_BLANK_FLIP bit of vblank ref-count if update is clean. */
	if (!tegra_dc_windows_are_dirty(dc))
		clear_bit(V_BLANK_FLIP, &dc->vblank_ref_count);

	/* Update the SD brightness */
	if (dc->out->sd_settings && !dc->out->sd_settings->use_vpulse2) {
		nvsd_updated = nvsd_update_brightness(dc);
		/* Ref-count vblank if nvsd is on-going. Otherwise, clean the
		 * V_BLANK_NVSD bit of vblank ref-count. */
		if (nvsd_updated) {
			set_bit(V_BLANK_NVSD, &dc->vblank_ref_count);
			tegra_dc_unmask_interrupt(dc, V_BLANK_INT);
		} else {
			clear_bit(V_BLANK_NVSD, &dc->vblank_ref_count);
		}
	}

	/* Mask vblank interrupt if ref-count is zero. */
	if (!dc->vblank_ref_count)
		tegra_dc_mask_interrupt(dc, V_BLANK_INT);

	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);

	/* Do the actual brightness update outside of the mutex dc->lock */
	if (nvsd_updated)
		tegra_dc_prism_update_backlight(dc);
}

static void tegra_dc_one_shot_worker(struct work_struct *work)
{
	struct tegra_dc *dc = container_of(
		to_delayed_work(work), struct tegra_dc, one_shot_work);
	mutex_lock(&dc->lock);

	/* memory client has gone idle */
	tegra_dc_clear_bandwidth(dc);

	if (dc->out_ops->idle) {
		tegra_dc_io_start(dc);
		dc->out_ops->idle(dc);
		tegra_dc_io_end(dc);
	}

	mutex_unlock(&dc->lock);
}

/* return an arbitrarily large number if count overflow occurs.
 * make it a nice base-10 number to show up in stats output */
static u64 tegra_dc_underflow_count(struct tegra_dc *dc, unsigned reg)
{
	unsigned count = tegra_dc_readl(dc, reg);

	tegra_dc_writel(dc, 0, reg);
	return ((count & 0x80000000) == 0) ? count : 10000000000ll;
}

static void tegra_dc_underflow_handler(struct tegra_dc *dc)
{
	int i;

	dc->stats.underflows++;
	if (dc->underflow_mask & WIN_A_UF_INT)
		dc->stats.underflows_a += tegra_dc_underflow_count(dc,
			DC_WINBUF_AD_UFLOW_STATUS);
	if (dc->underflow_mask & WIN_B_UF_INT)
		dc->stats.underflows_b += tegra_dc_underflow_count(dc,
			DC_WINBUF_BD_UFLOW_STATUS);
	if (dc->underflow_mask & WIN_C_UF_INT)
		dc->stats.underflows_c += tegra_dc_underflow_count(dc,
			DC_WINBUF_CD_UFLOW_STATUS);

	/* Check for any underflow reset conditions */
	for (i = 0; i < DC_N_WINDOWS; i++) {
		if (dc->underflow_mask & (WIN_A_UF_INT << i)) {
			dc->windows[i].underflows++;

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
			if (dc->windows[i].underflows > 4) {
				schedule_work(&dc->reset_work);
				/* reset counter */
				dc->windows[i].underflows = 0;
				trace_display_reset(dc);
			}
#endif
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
			if (dc->windows[i].underflows > 4) {
				trace_display_reset(dc);
				tegra_dc_writel(dc, UF_LINE_FLUSH,
						DC_DISP_DISP_MISC_CONTROL);
				tegra_dc_writel(dc, GENERAL_UPDATE,
						DC_CMD_STATE_CONTROL);
				tegra_dc_writel(dc, GENERAL_ACT_REQ,
						DC_CMD_STATE_CONTROL);

				tegra_dc_writel(dc, 0,
						DC_DISP_DISP_MISC_CONTROL);
				tegra_dc_writel(dc, GENERAL_UPDATE,
						DC_CMD_STATE_CONTROL);
				tegra_dc_writel(dc, GENERAL_ACT_REQ,
						DC_CMD_STATE_CONTROL);
			}
#endif
		} else {
			dc->windows[i].underflows = 0;
		}
	}

	/* Clear the underflow mask now that we've checked it. */
	tegra_dc_writel(dc, dc->underflow_mask, DC_CMD_INT_STATUS);
	dc->underflow_mask = 0;
	tegra_dc_unmask_interrupt(dc, ALL_UF_INT);
	trace_underflow(dc);
}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
static void tegra_dc_vpulse2(struct work_struct *work)
{
	struct tegra_dc *dc = container_of(work, struct tegra_dc, vpulse2_work);
	bool nvsd_updated = false;

	mutex_lock(&dc->lock);

	if (!dc->enabled) {
		mutex_unlock(&dc->lock);
		return;
	}

	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);

	/* Clear the V_PULSE2_FLIP if no update */
	if (!tegra_dc_windows_are_dirty(dc))
		clear_bit(V_PULSE2_FLIP, &dc->vpulse2_ref_count);

	/* Update the SD brightness */
	if (dc->out->sd_settings && dc->out->sd_settings->use_vpulse2) {
		nvsd_updated = nvsd_update_brightness(dc);
		if (nvsd_updated) {
			set_bit(V_PULSE2_NVSD, &dc->vpulse2_ref_count);
			tegra_dc_unmask_interrupt(dc, V_PULSE2_INT);
		} else {
			clear_bit(V_PULSE2_NVSD, &dc->vpulse2_ref_count);
		}
	}

	/* Mask vpulse2 interrupt if ref-count is zero. */
	if (!dc->vpulse2_ref_count)
		tegra_dc_mask_interrupt(dc, V_PULSE2_INT);

	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);

	/* Do the actual brightness update outside of the mutex dc->lock */
	if (nvsd_updated)
		tegra_dc_prism_update_backlight(dc);
}
#endif

#ifndef CONFIG_TEGRA_FPGA_PLATFORM
static void tegra_dc_one_shot_irq(struct tegra_dc *dc, unsigned long status)
{
	/* pending user vblank, so wakeup */
	if ((status & (V_BLANK_INT | MSF_INT)) &&
	    (dc->out->user_needs_vblank)) {
		dc->out->user_needs_vblank = false;
		complete(&dc->out->user_vblank_comp);
	}

	if (status & V_BLANK_INT) {
		/* Sync up windows. */
		tegra_dc_trigger_windows(dc);

		/* Schedule any additional bottom-half vblank actvities. */
		queue_work(system_freezable_wq, &dc->vblank_work);
	}

	if (status & FRAME_END_INT) {
		/* Mark the frame_end as complete. */
		if (!completion_done(&dc->frame_end_complete))
			complete(&dc->frame_end_complete);
	}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	if (status & V_PULSE2_INT)
		queue_work(system_freezable_wq, &dc->vpulse2_work);
#endif
}

static void tegra_dc_continuous_irq(struct tegra_dc *dc, unsigned long status)
{
	/* Schedule any additional bottom-half vblank actvities. */
	if (status & V_BLANK_INT)
		queue_work(system_freezable_wq, &dc->vblank_work);

	if (status & FRAME_END_INT) {
		struct timespec tm = CURRENT_TIME;
		dc->frame_end_timestamp = timespec_to_ns(&tm);
		wake_up(&dc->timestamp_wq);

		/* Mark the frame_end as complete. */
		if (!completion_done(&dc->frame_end_complete))
			complete(&dc->frame_end_complete);

		tegra_dc_trigger_windows(dc);
	}

#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	if (status & V_PULSE2_INT)
		queue_work(system_freezable_wq, &dc->vpulse2_work);
#endif
}

/* XXX: Not sure if we limit look ahead to 1 frame */
bool tegra_dc_is_within_n_vsync(struct tegra_dc *dc, s64 ts)
{
	BUG_ON(!dc->frametime_ns);
	return ((ts - dc->frame_end_timestamp) < dc->frametime_ns);
}

bool tegra_dc_does_vsync_separate(struct tegra_dc *dc, s64 new_ts, s64 old_ts)
{
	BUG_ON(!dc->frametime_ns);
	return (((new_ts - old_ts) > dc->frametime_ns)
		|| (div_s64((new_ts - dc->frame_end_timestamp), dc->frametime_ns)
			!= div_s64((old_ts - dc->frame_end_timestamp),
				dc->frametime_ns)));
}
#endif

static irqreturn_t tegra_dc_irq(int irq, void *ptr)
{
#ifndef CONFIG_TEGRA_FPGA_PLATFORM
	struct tegra_dc *dc = ptr;
	unsigned long status;
	unsigned long underflow_mask;
	u32 val;
	int need_disable = 0;

	mutex_lock(&dc->lock);
	if (!dc->enabled || !tegra_dc_is_powered(dc)) {
		mutex_unlock(&dc->lock);
		return IRQ_HANDLED;
	}

	clk_prepare_enable(dc->clk);
	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);

	if (!nvhost_module_powered_ext(dc->ndev)) {
		WARN(1, "IRQ when DC not powered!\n");
		status = tegra_dc_readl(dc, DC_CMD_INT_STATUS);
		tegra_dc_writel(dc, status, DC_CMD_INT_STATUS);
		tegra_dc_release_dc_out(dc);
		tegra_dc_io_end(dc);
		clk_disable_unprepare(dc->clk);
		mutex_unlock(&dc->lock);
		return IRQ_HANDLED;
	}

	/* clear all status flags except underflow, save those for the worker */
	status = tegra_dc_readl(dc, DC_CMD_INT_STATUS);
	tegra_dc_writel(dc, status & ~ALL_UF_INT, DC_CMD_INT_STATUS);
	val = tegra_dc_readl(dc, DC_CMD_INT_MASK);
	tegra_dc_writel(dc, val & ~ALL_UF_INT, DC_CMD_INT_MASK);

	/*
	 * Overlays can get thier internal state corrupted during and underflow
	 * condition.  The only way to fix this state is to reset the DC.
	 * if we get 4 consecutive frames with underflows, assume we're
	 * hosed and reset.
	 */
	underflow_mask = status & ALL_UF_INT;

	/* Check underflow */
	if (underflow_mask) {
		dc->underflow_mask |= underflow_mask;
		schedule_delayed_work(&dc->underflow_work,
			msecs_to_jiffies(1));
	}

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		tegra_dc_one_shot_irq(dc, status);
	else
		tegra_dc_continuous_irq(dc, status);

	/* update video mode if it has changed since the last frame */
	if (status & (FRAME_END_INT | V_BLANK_INT))
		if (tegra_dc_update_mode(dc))
			need_disable = 1; /* force display off on error */

	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	clk_disable_unprepare(dc->clk);
	mutex_unlock(&dc->lock);

	if (need_disable)
		tegra_dc_disable(dc);
	return IRQ_HANDLED;
#else /* CONFIG_TEGRA_FPGA_PLATFORM */
	return IRQ_NONE;
#endif /* !CONFIG_TEGRA_FPGA_PLATFORM */
}

void tegra_dc_set_color_control(struct tegra_dc *dc)
{
	u32 color_control;

	switch (dc->out->depth) {
	case 3:
		color_control = BASE_COLOR_SIZE111;
		break;

	case 6:
		color_control = BASE_COLOR_SIZE222;
		break;

	case 8:
		color_control = BASE_COLOR_SIZE332;
		break;

	case 9:
		color_control = BASE_COLOR_SIZE333;
		break;

	case 12:
		color_control = BASE_COLOR_SIZE444;
		break;

	case 15:
		color_control = BASE_COLOR_SIZE555;
		break;

	case 16:
		color_control = BASE_COLOR_SIZE565;
		break;

	case 18:
		color_control = BASE_COLOR_SIZE666;
		break;

	default:
		color_control = BASE_COLOR_SIZE888;
		break;
	}

	switch (dc->out->dither) {
	case TEGRA_DC_DISABLE_DITHER:
		color_control |= DITHER_CONTROL_DISABLE;
		break;
	case TEGRA_DC_ORDERED_DITHER:
		color_control |= DITHER_CONTROL_ORDERED;
		break;
	case TEGRA_DC_ERRDIFF_DITHER:
		/* The line buffer for error-diffusion dither is limited
		 * to 1280 pixels per line. This limits the maximum
		 * horizontal active area size to 1280 pixels when error
		 * diffusion is enabled.
		 */
		BUG_ON(dc->mode.h_active > 1280);
		color_control |= DITHER_CONTROL_ERRDIFF;
		break;
	}

#ifdef CONFIG_TEGRA_DC_CMU
	if (dc->pdata->flags & TEGRA_DC_FLAG_CMU_ENABLE)
		color_control |= CMU_ENABLE;
#endif

	tegra_dc_writel(dc, color_control, DC_DISP_DISP_COLOR_CONTROL);
}

static u32 get_syncpt(struct tegra_dc *dc, int idx)
{
	if (idx >= 0 && idx < ARRAY_SIZE(dc->win_syncpt))
		return dc->win_syncpt[idx];
	BUG();
}

static void tegra_dc_init_vpulse2_int(struct tegra_dc *dc)
{
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	u32 start, end;
	unsigned long val;

	val = V_PULSE2_H_POSITION(0) | V_PULSE2_LAST(0x1);
	tegra_dc_writel(dc, val, DC_DISP_V_PULSE2_CONTROL);

	start = dc->mode.v_ref_to_sync + dc->mode.v_sync_width +
		dc->mode.v_back_porch +	dc->mode.v_active;
	end = start + 1;
	val = V_PULSE2_START_A(start) + V_PULSE2_END_A(end);
	tegra_dc_writel(dc, val, DC_DISP_V_PULSE2_POSITION_A);

	val = tegra_dc_readl(dc, DC_CMD_INT_ENABLE);
	val |= V_PULSE2_INT;
	tegra_dc_writel(dc, val , DC_CMD_INT_ENABLE);

	tegra_dc_mask_interrupt(dc, V_PULSE2_INT);
	tegra_dc_writel(dc, V_PULSE_2_ENABLE, DC_DISP_DISP_SIGNAL_OPTIONS0);
#endif
}

static int tegra_dc_init(struct tegra_dc *dc)
{
	int i;
	int int_enable;

	tegra_dc_io_start(dc);
	tegra_dc_writel(dc, 0x00000100, DC_CMD_GENERAL_INCR_SYNCPT_CNTRL);
	if (dc->ndev->id == 0) {
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0A,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0B,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0C,
				      TEGRA_MC_PRIO_MED);
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
		/* only present on Tegra2 and 3 */
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY1B,
				      TEGRA_MC_PRIO_MED);
#endif
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAYHC,
				      TEGRA_MC_PRIO_HIGH);
	} else if (dc->ndev->id == 1) {
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0AB,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0BB,
				      TEGRA_MC_PRIO_MED);
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY0CB,
				      TEGRA_MC_PRIO_MED);
#if defined(CONFIG_ARCH_TEGRA_2x_SOC) || defined(CONFIG_ARCH_TEGRA_3x_SOC)
		/* only present on Tegra2 and 3 */
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAY1BB,
				      TEGRA_MC_PRIO_MED);
#endif
		tegra_mc_set_priority(TEGRA_MC_CLIENT_DISPLAYHCB,
				      TEGRA_MC_PRIO_HIGH);
	}
	tegra_dc_writel(dc, 0x00000100 | dc->vblank_syncpt,
			DC_CMD_CONT_SYNCPT_VSYNC);

	tegra_dc_writel(dc, 0x00004700, DC_CMD_INT_TYPE);
	tegra_dc_writel(dc, 0x0001c700, DC_CMD_INT_POLARITY);
	tegra_dc_writel(dc, 0x00202020, DC_DISP_MEM_HIGH_PRIORITY);
	tegra_dc_writel(dc, 0x00010101, DC_DISP_MEM_HIGH_PRIORITY_TIMER);
#ifdef CONFIG_ARCH_TEGRA_3x_SOC
	tegra_dc_writel(dc, 0x00000000, DC_DISP_DISP_MISC_CONTROL);
#endif
	/* enable interrupts for vblank, frame_end and underflows */
	int_enable = (FRAME_END_INT | V_BLANK_INT | ALL_UF_INT);
	/* for panels with one-shot mode enable tearing effect interrupt */
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		int_enable |= MSF_INT;

	tegra_dc_writel(dc, int_enable, DC_CMD_INT_ENABLE);
	tegra_dc_writel(dc, ALL_UF_INT, DC_CMD_INT_MASK);
	tegra_dc_init_vpulse2_int(dc);

	tegra_dc_writel(dc, 0x00000000, DC_DISP_BORDER_COLOR);

#ifdef CONFIG_TEGRA_DC_CMU
	if (dc->pdata->cmu)
		_tegra_dc_update_cmu(dc, dc->pdata->cmu);
	else
		_tegra_dc_update_cmu(dc, &default_cmu);
#endif
	tegra_dc_set_color_control(dc);
	for (i = 0; i < DC_N_WINDOWS; i++) {
		struct tegra_dc_win *win = &dc->windows[i];
		tegra_dc_writel(dc, WINDOW_A_SELECT << i,
				DC_CMD_DISPLAY_WINDOW_HEADER);
		tegra_dc_set_csc(dc, &win->csc);
		tegra_dc_set_lut(dc, win);
		tegra_dc_set_scaling_filter(dc);
	}

	for (i = 0; i < dc->n_windows; i++) {
		u32 syncpt = get_syncpt(dc, i);

		dc->syncpt[i].id = syncpt;

		dc->syncpt[i].min = dc->syncpt[i].max =
			nvhost_syncpt_read_ext(dc->ndev, syncpt);
	}

	trace_display_mode(dc, &dc->mode);

	if (dc->mode.pclk) {
		if (tegra_dc_program_mode(dc, &dc->mode)) {
			tegra_dc_io_end(dc);
			return -EINVAL;
		}
	}

	/* Initialize SD AFTER the modeset.
	   nvsd_init handles the sd_settings = NULL case. */
	nvsd_init(dc, dc->out->sd_settings);

	tegra_dc_io_end(dc);

	return 0;
}

static bool _tegra_dc_controller_enable(struct tegra_dc *dc)
{
	int failed_init = 0;

	tegra_dc_unpowergate_locked(dc);

	if (dc->out->enable)
		dc->out->enable(&dc->ndev->dev);

	tegra_dc_setup_clk(dc, dc->clk);
	tegra_dc_clk_enable(dc);
	tegra_dc_io_start(dc);

	tegra_dc_power_on(dc);

	/* do not accept interrupts during initialization */
	tegra_dc_writel(dc, 0, DC_CMD_INT_MASK);

	enable_dc_irq(dc);

	failed_init = tegra_dc_init(dc);
	if (failed_init) {
		tegra_dc_writel(dc, 0, DC_CMD_INT_MASK);
		disable_irq_nosync(dc->irq);
		tegra_dc_clear_bandwidth(dc);
		tegra_dc_clk_disable(dc);
		if (dc->out && dc->out->disable)
			dc->out->disable();
		tegra_dc_io_end(dc);
		return false;
	}

	if (dc->out_ops && dc->out_ops->enable)
		dc->out_ops->enable(dc);

	/* force a full blending update */
	dc->blend.z[0] = -1;

	tegra_dc_ext_enable(dc->ext);

	trace_display_enable(dc);

	tegra_dc_writel(dc, GENERAL_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, GENERAL_ACT_REQ, DC_CMD_STATE_CONTROL);

	if (dc->out->postpoweron)
		dc->out->postpoweron();

	tegra_log_resume_time();

	tegra_dc_io_end(dc);
	return true;
}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static bool _tegra_dc_controller_reset_enable(struct tegra_dc *dc)
{
	bool ret = true;

	if (dc->out->enable)
		dc->out->enable(&dc->ndev->dev);

	tegra_dc_setup_clk(dc, dc->clk);
	tegra_dc_clk_enable(dc);

	if (dc->ndev->id == 0 && tegra_dcs[1] != NULL) {
		mutex_lock(&tegra_dcs[1]->lock);
		disable_irq_nosync(tegra_dcs[1]->irq);
	} else if (dc->ndev->id == 1 && tegra_dcs[0] != NULL) {
		mutex_lock(&tegra_dcs[0]->lock);
		disable_irq_nosync(tegra_dcs[0]->irq);
	}

	msleep(5);
	tegra_periph_reset_assert(dc->clk);
	msleep(2);
#ifdef CONFIG_TEGRA_SILICON_PLATFORM
	tegra_periph_reset_deassert(dc->clk);
	msleep(1);
#endif

	if (dc->ndev->id == 0 && tegra_dcs[1] != NULL) {
		enable_dc_irq(tegra_dcs[1]);
		mutex_unlock(&tegra_dcs[1]->lock);
	} else if (dc->ndev->id == 1 && tegra_dcs[0] != NULL) {
		enable_dc_irq(tegra_dcs[0]);
		mutex_unlock(&tegra_dcs[0]->lock);
	}

	enable_dc_irq(dc);

	if (tegra_dc_init(dc)) {
		dev_err(&dc->ndev->dev, "cannot initialize\n");
		ret = false;
	}

	if (dc->out_ops && dc->out_ops->enable)
		dc->out_ops->enable(dc);

	if (dc->out->postpoweron)
		dc->out->postpoweron();

	/* force a full blending update */
	dc->blend.z[0] = -1;

	tegra_dc_ext_enable(dc->ext);

	if (!ret) {
		dev_err(&dc->ndev->dev, "initialization failed,disabling");
		_tegra_dc_controller_disable(dc);
	}

	trace_display_reset(dc);
	return ret;
}
#endif

static int _tegra_dc_set_default_videomode(struct tegra_dc *dc)
{
	if (dc->mode.pclk == 0) {
		switch (dc->out->type) {
		case TEGRA_DC_OUT_HDMI:
		/* DC enable called but no videomode is loaded.
		     Check if HDMI is connected, then set fallback mdoe */
		if (tegra_dc_hpd(dc)) {
			return tegra_dc_set_fb_mode(dc,
					&tegra_dc_hdmi_fallback_mode, 0);
		} else
			return false;

		break;

		/* Do nothing for other outputs for now */
		case TEGRA_DC_OUT_RGB:

		case TEGRA_DC_OUT_DSI:

		default:
			return false;
		}
	}

	return false;
}

int tegra_dc_set_default_videomode(struct tegra_dc *dc)
{
	return _tegra_dc_set_default_videomode(dc);
}

static bool _tegra_dc_enable(struct tegra_dc *dc)
{
	if (dc->mode.pclk == 0)
		return false;

	if (!dc->out)
		return false;

	if (dc->enabled)
		return true;

	if (!_tegra_dc_controller_enable(dc))
		return false;

	return true;
}

void tegra_dc_enable(struct tegra_dc *dc)
{
	mutex_lock(&dc->lock);

	if (!dc->enabled)
		dc->enabled = _tegra_dc_enable(dc);

	mutex_unlock(&dc->lock);
	trace_display_mode(dc, &dc->mode);
}

static void _tegra_dc_controller_disable(struct tegra_dc *dc)
{
	unsigned i;

	tegra_dc_hold_dc_out(dc);

	if (dc->out && dc->out->prepoweroff)
		dc->out->prepoweroff();

	if (dc->out_ops && dc->out_ops->disable)
		dc->out_ops->disable(dc);

	tegra_dc_writel(dc, 0, DC_CMD_INT_MASK);

	disable_irq_nosync(dc->irq);

	tegra_dc_clear_bandwidth(dc);

	if (dc->out && dc->out->disable)
		dc->out->disable();

	for (i = 0; i < dc->n_windows; i++) {
		struct tegra_dc_win *w = &dc->windows[i];

		/* reset window bandwidth */
		w->bandwidth = 0;
		w->new_bandwidth = 0;

		/* disable windows */
		w->flags &= ~TEGRA_WIN_FLAG_ENABLED;

		/* set window physical address to invalid*/
		w->phys_addr = 0;

		/* flush any pending syncpt waits */
		while (dc->syncpt[i].min < dc->syncpt[i].max) {
			trace_display_syncpt_flush(dc, dc->syncpt[i].id,
				dc->syncpt[i].min, dc->syncpt[i].max);
			dc->syncpt[i].min++;
			nvhost_syncpt_cpu_incr_ext(dc->ndev, dc->syncpt[i].id);
		}
	}
	trace_display_disable(dc);

	tegra_dc_clk_disable(dc);
	tegra_dc_release_dc_out(dc);
}

void tegra_dc_stats_enable(struct tegra_dc *dc, bool enable)
{
#if 0 /* underflow interrupt is already enabled by dc reset worker */
	u32 val;
	if (dc->enabled)  {
		val = tegra_dc_readl(dc, DC_CMD_INT_ENABLE);
		if (enable)
			val |= (WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT);
		else
			val &= ~(WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT);
		tegra_dc_writel(dc, val, DC_CMD_INT_ENABLE);
	}
#endif
}

bool tegra_dc_stats_get(struct tegra_dc *dc)
{
#if 0 /* right now it is always enabled */
	u32 val;
	bool res;

	if (dc->enabled)  {
		val = tegra_dc_readl(dc, DC_CMD_INT_ENABLE);
		res = !!(val & (WIN_A_UF_INT | WIN_B_UF_INT | WIN_C_UF_INT));
	} else {
		res = false;
	}

	return res;
#endif
	return true;
}

/* make the screen blank by disabling all windows */
void tegra_dc_blank(struct tegra_dc *dc)
{
	struct tegra_dc_win *dcwins[DC_N_WINDOWS];
	unsigned i;

	for (i = 0; i < DC_N_WINDOWS; i++) {
		dcwins[i] = tegra_dc_get_window(dc, i);
		dcwins[i]->flags &= ~TEGRA_WIN_FLAG_ENABLED;
	}

	tegra_dc_update_windows(dcwins, DC_N_WINDOWS);
	tegra_dc_sync_windows(dcwins, DC_N_WINDOWS);
}

static void _tegra_dc_disable(struct tegra_dc *dc)
{
	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE) {
		mutex_lock(&dc->one_shot_lock);
		cancel_delayed_work_sync(&dc->one_shot_work);
	}

	tegra_dc_io_start(dc);
	_tegra_dc_controller_disable(dc);
	tegra_dc_io_end(dc);

	tegra_dc_powergate_locked(dc);

	if (dc->out->flags & TEGRA_DC_OUT_ONE_SHOT_MODE)
		mutex_unlock(&dc->one_shot_lock);

	/*
	 * We will need to reinitialize the display the next time panel
	 * is enabled.
	 */
	dc->out->flags &= ~TEGRA_DC_OUT_INITIALIZED_MODE;

	tegra_log_suspend_time();
}

void tegra_dc_disable(struct tegra_dc *dc)
{
	tegra_dc_ext_disable(dc->ext);

	/* it's important that new underflow work isn't scheduled before the
	 * lock is acquired. */
	cancel_delayed_work_sync(&dc->underflow_work);

	mutex_lock(&dc->lock);

	if (dc->enabled) {
		dc->enabled = false;

		if (!dc->suspended)
			_tegra_dc_disable(dc);
	}

#ifdef CONFIG_SWITCH
	switch_set_state(&dc->modeset_switch, 0);
#endif

	mutex_unlock(&dc->lock);
	synchronize_irq(dc->irq);
	trace_display_mode(dc, &dc->mode);
}

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
static void tegra_dc_reset_worker(struct work_struct *work)
{
	struct tegra_dc *dc =
		container_of(work, struct tegra_dc, reset_work);

	unsigned long val = 0;

	mutex_lock(&shared_lock);

	dev_warn(&dc->ndev->dev,
		"overlay stuck in underflow state.  resetting.\n");

	tegra_dc_ext_disable(dc->ext);

	mutex_lock(&dc->lock);

	if (dc->enabled == false)
		goto unlock;

	dc->enabled = false;

	/*
	 * off host read bus
	 */
	val = tegra_dc_readl(dc, DC_CMD_CONT_SYNCPT_VSYNC);
	val &= ~(0x00000100);
	tegra_dc_writel(dc, val, DC_CMD_CONT_SYNCPT_VSYNC);

	/*
	 * set DC to STOP mode
	 */
	tegra_dc_writel(dc, DISP_CTRL_MODE_STOP, DC_CMD_DISPLAY_COMMAND);

	msleep(10);

	_tegra_dc_controller_disable(dc);

	/* _tegra_dc_controller_reset_enable deasserts reset */
	_tegra_dc_controller_reset_enable(dc);

	dc->enabled = true;

	/* reopen host read bus */
	val = tegra_dc_readl(dc, DC_CMD_CONT_SYNCPT_VSYNC);
	val &= ~(0x00000100);
	val |= 0x100;
	tegra_dc_writel(dc, val, DC_CMD_CONT_SYNCPT_VSYNC);

unlock:
	mutex_unlock(&dc->lock);
	mutex_unlock(&shared_lock);
	trace_display_reset(dc);
}
#endif

static void tegra_dc_underflow_worker(struct work_struct *work)
{
	struct tegra_dc *dc = container_of(
		to_delayed_work(work), struct tegra_dc, underflow_work);

	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);
	tegra_dc_hold_dc_out(dc);

	if (dc->enabled) {
		tegra_dc_underflow_handler(dc);
	}
	tegra_dc_release_dc_out(dc);
	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);
}

#ifdef CONFIG_SWITCH
static ssize_t switch_modeset_print_mode(struct switch_dev *sdev, char *buf)
{
	struct tegra_dc *dc =
		container_of(sdev, struct tegra_dc, modeset_switch);

	if (!sdev->state)
		return sprintf(buf, "offline\n");

	return sprintf(buf, "%dx%d\n", dc->mode.h_active, dc->mode.v_active);
}
#endif

static void tegra_dc_add_modes(struct tegra_dc *dc)
{
	struct fb_monspecs specs;
	int i;

	memset(&specs, 0, sizeof(specs));
	specs.max_x = dc->mode.h_active * 1000;
	specs.max_y = dc->mode.v_active * 1000;
	specs.modedb_len = dc->out->n_modes;
	specs.modedb = kzalloc(specs.modedb_len *
		sizeof(struct fb_videomode), GFP_KERNEL);
	if (specs.modedb == NULL) {
		dev_err(&dc->ndev->dev, "modedb allocation failed\n");
		return;
	}
	for (i = 0; i < dc->out->n_modes; i++)
		tegra_dc_to_fb_videomode(&specs.modedb[i],
			&dc->out->modes[i]);
	tegra_fb_update_monspecs(dc->fb, &specs, NULL);
}

static int tegra_dc_probe(struct platform_device *ndev)
{
	struct tegra_dc *dc;
	struct tegra_dc_mode *mode;
	struct clk *clk;
	struct clk *emc_clk;
	struct resource	*res;
	struct resource *base_res;
	struct resource *fb_mem = NULL;
	int ret = 0;
	void __iomem *base;
	int irq;
	int i;

	if (!ndev->dev.platform_data) {
		dev_err(&ndev->dev, "no platform data\n");
		return -ENOENT;
	}

	dc = kzalloc(sizeof(struct tegra_dc), GFP_KERNEL);
	if (!dc) {
		dev_err(&ndev->dev, "can't allocate memory for tegra_dc\n");
		return -ENOMEM;
	}

	irq = platform_get_irq_byname(ndev, "irq");
	if (irq <= 0) {
		dev_err(&ndev->dev, "no irq\n");
		ret = -ENOENT;
		goto err_free;
	}

	res = platform_get_resource_byname(ndev, IORESOURCE_MEM, "regs");
	if (!res) {
		dev_err(&ndev->dev, "no mem resource\n");
		ret = -ENOENT;
		goto err_free;
	}

	base_res = request_mem_region(res->start, resource_size(res),
		ndev->name);
	if (!base_res) {
		dev_err(&ndev->dev, "request_mem_region failed\n");
		ret = -EBUSY;
		goto err_free;
	}

	base = ioremap(res->start, resource_size(res));
	if (!base) {
		dev_err(&ndev->dev, "registers can't be mapped\n");
		ret = -EBUSY;
		goto err_release_resource_reg;
	}
	if (TEGRA_DISPLAY_BASE == res->start) {
		dc->vblank_syncpt = NVSYNCPT_VBLANK0;
		dc->win_syncpt[0] = NVSYNCPT_DISP0_A;
		dc->win_syncpt[1] = NVSYNCPT_DISP0_B;
		dc->win_syncpt[2] = NVSYNCPT_DISP0_C;
		/* This code assumes DISB depends on DISA. DC's powergate
		 * code will have to change if dependency is removed */
		if (dc->out && dc->out->type == TEGRA_DC_OUT_HDMI)
			dc->powergate_id = TEGRA_POWERGATE_DISB;
		else
			dc->powergate_id = TEGRA_POWERGATE_DISA;
	} else if (TEGRA_DISPLAY2_BASE == res->start) {
		dc->vblank_syncpt = NVSYNCPT_VBLANK1;
		dc->win_syncpt[0] = NVSYNCPT_DISP1_A;
		dc->win_syncpt[1] = NVSYNCPT_DISP1_B;
		dc->win_syncpt[2] = NVSYNCPT_DISP1_C;
		dc->powergate_id = TEGRA_POWERGATE_DISB;
	} else {
		dev_err(&ndev->dev,
			"Unknown base address %#08x: unable to assign syncpt\n",
			res->start);
	}


	fb_mem = platform_get_resource_byname(ndev, IORESOURCE_MEM, "fbmem");

	clk = clk_get(&ndev->dev, NULL);
	if (IS_ERR_OR_NULL(clk)) {
		dev_err(&ndev->dev, "can't get clock\n");
		ret = -ENOENT;
		goto err_iounmap_reg;
	}

	emc_clk = clk_get(&ndev->dev, "emc");
	if (IS_ERR_OR_NULL(emc_clk)) {
		dev_err(&ndev->dev, "can't get emc clock\n");
		ret = -ENOENT;
		goto err_put_clk;
	}

	dc->clk = clk;
	dc->emc_clk = emc_clk;
	dc->shift_clk_div.mul = dc->shift_clk_div.div = 1;
	/* Initialize one shot work delay, it will be assigned by dsi
	 * according to refresh rate later. */
	dc->one_shot_delay_ms = 40;

	dc->base_res = base_res;
	dc->base = base;
	dc->irq = irq;
	dc->ndev = ndev;
	dc->pdata = ndev->dev.platform_data;

	/*
	 * The emc is a shared clock, it will be set based on
	 * the requirements for each user on the bus.
	 */
	dc->emc_clk_rate = 0;

	mutex_init(&dc->lock);
	mutex_init(&dc->one_shot_lock);
	init_completion(&dc->frame_end_complete);
	init_waitqueue_head(&dc->wq);
	init_waitqueue_head(&dc->timestamp_wq);
#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	INIT_WORK(&dc->reset_work, tegra_dc_reset_worker);
#endif
	INIT_WORK(&dc->vblank_work, tegra_dc_vblank);
	dc->vblank_ref_count = 0;
#if !defined(CONFIG_ARCH_TEGRA_2x_SOC) && !defined(CONFIG_ARCH_TEGRA_3x_SOC)
	INIT_WORK(&dc->vpulse2_work, tegra_dc_vpulse2);
#endif
	dc->vpulse2_ref_count = 0;
	INIT_DELAYED_WORK(&dc->underflow_work, tegra_dc_underflow_worker);
	INIT_DELAYED_WORK(&dc->one_shot_work, tegra_dc_one_shot_worker);

	tegra_dc_init_lut_defaults(&dc->fb_lut);

	dc->n_windows = DC_N_WINDOWS;
	for (i = 0; i < dc->n_windows; i++) {
		struct tegra_dc_win *win = &dc->windows[i];
		win->idx = i;
		win->dc = dc;
		tegra_dc_init_csc_defaults(&win->csc);
		tegra_dc_init_lut_defaults(&win->lut);
	}

	ret = tegra_dc_set(dc, ndev->id);
	if (ret < 0) {
		dev_err(&ndev->dev, "can't add dc\n");
		goto err_put_emc_clk;
	}

	platform_set_drvdata(ndev, dc);

#ifdef CONFIG_SWITCH
	dc->modeset_switch.name = dev_name(&ndev->dev);
	dc->modeset_switch.state = 0;
	dc->modeset_switch.print_state = switch_modeset_print_mode;
	ret = switch_dev_register(&dc->modeset_switch);
	if (ret < 0)
		dev_err(&ndev->dev, "failed to register switch driver\n");
#endif

	tegra_dc_feature_register(dc);

	if (dc->pdata->default_out)
		tegra_dc_set_out(dc, dc->pdata->default_out);
	else
		dev_err(&ndev->dev, "No default output specified.  Leaving output disabled.\n");
	dc->mode_dirty = false; /* ignore changes tegra_dc_set_out has done */

	dc->ext = tegra_dc_ext_register(ndev, dc);
	if (IS_ERR_OR_NULL(dc->ext)) {
		dev_warn(&ndev->dev, "Failed to enable Tegra DC extensions.\n");
		dc->ext = NULL;
	}

	/* interrupt handler must be registered before tegra_fb_register() */
	if (request_threaded_irq(irq, NULL, tegra_dc_irq, IRQF_ONESHOT,
			dev_name(&ndev->dev), dc)) {
		dev_err(&ndev->dev, "request_irq %d failed\n", irq);
		ret = -EBUSY;
		goto err_disable_dc;
	}
	disable_dc_irq(dc);

	if (dc->pdata->flags & TEGRA_DC_FLAG_ENABLED) {
		_tegra_dc_set_default_videomode(dc);
		dc->enabled = _tegra_dc_enable(dc);
	}

	tegra_dc_create_debugfs(dc);

	dev_info(&ndev->dev, "probed\n");

	if (dc->pdata->fb) {
		if (dc->enabled && dc->pdata->fb->bits_per_pixel == -1) {
			unsigned long fmt;
			tegra_dc_writel(dc,
					WINDOW_A_SELECT << dc->pdata->fb->win,
					DC_CMD_DISPLAY_WINDOW_HEADER);

			fmt = tegra_dc_readl(dc, DC_WIN_COLOR_DEPTH);
			dc->pdata->fb->bits_per_pixel =
				tegra_dc_fmt_bpp(fmt);
		}

		mode = tegra_dc_get_override_mode(dc);
		if (mode) {
			dc->pdata->fb->xres = mode->h_active;
			dc->pdata->fb->yres = mode->v_active;
		}

		tegra_dc_io_start(dc);
		dc->fb = tegra_fb_register(ndev, dc, dc->pdata->fb, fb_mem);
		tegra_dc_io_end(dc);
		if (IS_ERR_OR_NULL(dc->fb)) {
			dc->fb = NULL;
			dev_err(&ndev->dev, "failed to register fb\n");
			goto err_remove_debugfs;
		}
	}

	if (dc->out && dc->out->n_modes)
		tegra_dc_add_modes(dc);

	tegra_dc_hotplug_init(dc);

	if (dc->out_ops && dc->out_ops->detect)
		dc->out_ops->detect(dc);
	else
		dc->connected = true;

	/* Powergate display module when it's unconnected. */
	if (!tegra_dc_get_connected(dc))
		tegra_dc_powergate_locked(dc);

	tegra_dc_create_sysfs(&dc->ndev->dev);

	return 0;

err_remove_debugfs:
	tegra_dc_remove_debugfs(dc);
	free_irq(irq, dc);
err_disable_dc:
	if (dc->ext) {
		tegra_dc_ext_disable(dc->ext);
		tegra_dc_ext_unregister(dc->ext);
	}
	mutex_lock(&dc->lock);
	if (dc->enabled)
		_tegra_dc_disable(dc);
	dc->enabled = false;
	mutex_unlock(&dc->lock);
#ifdef CONFIG_SWITCH
	switch_dev_unregister(&dc->modeset_switch);
#endif
err_put_emc_clk:
	clk_put(emc_clk);
err_put_clk:
	clk_put(clk);
err_iounmap_reg:
	iounmap(base);
	if (fb_mem)
		release_resource(fb_mem);
err_release_resource_reg:
	release_resource(base_res);
err_free:
	kfree(dc);

	return ret;
}

static int __devexit tegra_dc_remove(struct platform_device *ndev)
{
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	tegra_dc_remove_sysfs(&dc->ndev->dev);
	tegra_dc_remove_debugfs(dc);

	if (dc->fb) {
		tegra_fb_unregister(dc->fb);
		if (dc->fb_mem)
			release_resource(dc->fb_mem);
	}

	tegra_dc_ext_disable(dc->ext);

	if (dc->ext)
		tegra_dc_ext_unregister(dc->ext);

	mutex_lock(&dc->lock);
	if (dc->enabled)
		_tegra_dc_disable(dc);
	dc->enabled = false;
	mutex_unlock(&dc->lock);
	synchronize_irq(dc->irq); /* wait for IRQ handlers to finish */

#ifdef CONFIG_SWITCH
	switch_dev_unregister(&dc->modeset_switch);
#endif
	free_irq(dc->irq, dc);
	clk_put(dc->emc_clk);
	clk_put(dc->clk);
	iounmap(dc->base);
	if (dc->fb_mem)
		release_resource(dc->base_res);
	kfree(dc);
	tegra_dc_set(NULL, ndev->id);

	return 0;
}

#ifdef CONFIG_PM
static int tegra_dc_suspend(struct platform_device *ndev, pm_message_t state)
{
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	trace_display_suspend(dc);
	dev_info(&ndev->dev, "suspend\n");

	tegra_dc_ext_disable(dc->ext);

	mutex_lock(&dc->lock);
	tegra_dc_io_start(dc);

	if (dc->out_ops && dc->out_ops->suspend)
		dc->out_ops->suspend(dc);

	if (dc->enabled) {
		_tegra_dc_disable(dc);

		dc->suspended = true;
	}

	if (dc->out && dc->out->postsuspend)
		dc->out->postsuspend();

	tegra_dc_io_end(dc);
	mutex_unlock(&dc->lock);
	synchronize_irq(dc->irq); /* wait for IRQ handlers to finish */

	return 0;
}

static int tegra_dc_resume(struct platform_device *ndev)
{
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	trace_display_resume(dc);
	dev_info(&ndev->dev, "resume\n");

	mutex_lock(&dc->lock);
	dc->suspended = false;

	/* To pan the fb on resume */
	tegra_fb_pan_display_reset(dc->fb);

	if (dc->enabled) {
		dc->enabled = false;
		_tegra_dc_set_default_videomode(dc);
		dc->enabled = _tegra_dc_enable(dc);
	}

	if (dc->out_ops && dc->out_ops->resume)
		dc->out_ops->resume(dc);
	mutex_unlock(&dc->lock);

	return 0;
}

#endif /* CONFIG_PM */

static void tegra_dc_shutdown(struct platform_device *ndev)
{
	struct tegra_dc *dc = platform_get_drvdata(ndev);

	if (!dc || !dc->enabled)
		return;

	tegra_dc_disable(dc);
}

extern int suspend_set(const char *val, struct kernel_param *kp)
{
	if (!strcmp(val, "dump"))
		dump_regs(tegra_dcs[0]);
#ifdef CONFIG_PM
	else if (!strcmp(val, "suspend"))
		tegra_dc_suspend(tegra_dcs[0]->ndev, PMSG_SUSPEND);
	else if (!strcmp(val, "resume"))
		tegra_dc_resume(tegra_dcs[0]->ndev);
#endif

	return 0;
}

extern int suspend_get(char *buffer, struct kernel_param *kp)
{
	return 0;
}

int suspend;

module_param_call(suspend, suspend_set, suspend_get, &suspend, 0644);

struct platform_driver tegra_dc_driver = {
	.driver = {
		.name = "tegradc",
		.owner = THIS_MODULE,
	},
	.probe = tegra_dc_probe,
	.remove = tegra_dc_remove,
#ifdef CONFIG_PM
	.suspend = tegra_dc_suspend,
	.resume = tegra_dc_resume,
#endif
	.shutdown = tegra_dc_shutdown,
};

#ifndef MODULE
static int __init parse_disp_params(char *options, struct tegra_dc_mode *mode)
{
	int i, params[11];
	char *p;

	for (i = 0; i < ARRAY_SIZE(params); i++) {
		if ((p = strsep(&options, ",")) != NULL) {
			if (*p)
				params[i] = simple_strtoul(p, &p, 10);
		} else
			return -EINVAL;
	}

	if ((mode->pclk = params[0]) == 0)
		return -EINVAL;

	mode->h_active      = params[1];
	mode->v_active      = params[2];
	mode->h_ref_to_sync = params[3];
	mode->v_ref_to_sync = params[4];
	mode->h_sync_width  = params[5];
	mode->v_sync_width  = params[6];
	mode->h_back_porch  = params[7];
	mode->v_back_porch  = params[8];
	mode->h_front_porch = params[9];
	mode->v_front_porch = params[10];

	return 0;
}

static int __init tegra_dc_mode_override(char *str)
{
	char *p = str, *options;

	if (!p || !*p)
		return -EINVAL;

	p = strstr(str, "hdmi:");
	if (p) {
		p += 5;
		options = strsep(&p, ";");
		if (parse_disp_params(options, &override_disp_mode[TEGRA_DC_OUT_HDMI]))
			return -EINVAL;
	}

	p = strstr(str, "rgb:");
	if (p) {
		p += 4;
		options = strsep(&p, ";");
		if (parse_disp_params(options, &override_disp_mode[TEGRA_DC_OUT_RGB]))
			return -EINVAL;
	}

	p = strstr(str, "dsi:");
	if (p) {
		p += 4;
		options = strsep(&p, ";");
		if (parse_disp_params(options, &override_disp_mode[TEGRA_DC_OUT_DSI]))
			return -EINVAL;
	}

	return 0;
}

__setup("disp_params=", tegra_dc_mode_override);
#endif

static int __init tegra_dc_module_init(void)
{
	int ret = tegra_dc_ext_module_init();
	if (ret)
		return ret;
	return platform_driver_register(&tegra_dc_driver);
}

static void __exit tegra_dc_module_exit(void)
{
	platform_driver_unregister(&tegra_dc_driver);
	tegra_dc_ext_module_exit();
}

module_exit(tegra_dc_module_exit);
module_init(tegra_dc_module_init);
