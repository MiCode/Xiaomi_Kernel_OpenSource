/*
 * arch/arm/mach-tegra/board-enterprise-panel.c
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <linux/platform_device.h>
#include <linux/tegra_pwm_bl.h>
#include <linux/pwm_backlight.h>
#include <linux/nvhost.h>
#include <linux/nvmap.h>

#include <asm/mach-types.h>
#include <asm/atomic.h>

#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>
#include <mach/hardware.h>
#include <mach/gpio-tegra.h>

#include "board.h"
#include "board-enterprise.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "tegra3_host1x_devices.h"

#define DC_CTRL_MODE    TEGRA_DC_OUT_ONE_SHOT_MODE

/* Select panel to be used. */
#define AVDD_LCD PMU_TCA6416_GPIO_PORT17
#define DSI_PANEL_RESET 1

#define enterprise_lvds_shutdown	TEGRA_GPIO_PL2
#define enterprise_hdmi_hpd		TEGRA_GPIO_PN7

#define enterprise_dsi_panel_reset	TEGRA_GPIO_PW0

#define enterprise_lcd_2d_3d		TEGRA_GPIO_PH1
#define ENTERPRISE_STEREO_3D		0
#define ENTERPRISE_STEREO_2D		1

#define enterprise_lcd_swp_pl		TEGRA_GPIO_PH2
#define ENTERPRISE_STEREO_LANDSCAPE	0
#define ENTERPRISE_STEREO_PORTRAIT	1

#define enterprise_lcd_te		TEGRA_GPIO_PJ1

#define enterprise_bl_pwm		TEGRA_GPIO_PH3

#ifdef CONFIG_TEGRA_DC
static struct regulator *enterprise_dsi_reg;
static bool dsi_regulator_status;
static struct regulator *enterprise_lcd_reg;

static struct regulator *enterprise_hdmi_reg;
static struct regulator *enterprise_hdmi_pll;
static struct regulator *enterprise_hdmi_vddio;
#endif

static atomic_t sd_brightness = ATOMIC_INIT(255);

static tegra_dc_bl_output enterprise_bl_output_measured_a02 = {
	1, 5, 9, 10, 11, 12, 12, 13,
	13, 14, 14, 15, 15, 16, 16, 17,
	17, 18, 18, 19, 19, 20, 21, 21,
	22, 22, 23, 24, 24, 25, 26, 26,
	27, 27, 28, 29, 29, 31, 31, 32,
	32, 33, 34, 35, 36, 36, 37, 38,
	39, 39, 40, 41, 41, 42, 43, 43,
	44, 45, 45, 46, 47, 47, 48, 49,
	49, 50, 51, 51, 52, 53, 53, 54,
	55, 56, 56, 57, 58, 59, 60, 61,
	61, 62, 63, 64, 65, 65, 66, 67,
	67, 68, 69, 69, 70, 71, 71, 72,
	73, 73, 74, 74, 75, 76, 76, 77,
	77, 78, 79, 79, 80, 81, 82, 83,
	83, 84, 85, 85, 86, 86, 88, 89,
	90, 91, 91, 92, 93, 93, 94, 95,
	95, 96, 97, 97, 98, 99, 99, 100,
	101, 101, 102, 103, 103, 104, 105, 105,
	107, 107, 108, 109, 110, 111, 111, 112,
	113, 113, 114, 115, 115, 116, 117, 117,
	118, 119, 119, 120, 121, 122, 123, 124,
	124, 125, 126, 126, 127, 128, 129, 129,
	130, 131, 131, 132, 133, 133, 134, 135,
	135, 136, 137, 137, 138, 139, 139, 140,
	142, 142, 143, 144, 145, 146, 147, 147,
	148, 149, 149, 150, 151, 152, 153, 153,
	153, 154, 155, 156, 157, 158, 158, 159,
	160, 161, 162, 163, 163, 164, 165, 165,
	166, 166, 167, 168, 169, 169, 170, 170,
	171, 172, 173, 173, 174, 175, 175, 176,
	176, 178, 178, 179, 180, 181, 182, 182,
	183, 184, 185, 186, 186, 187, 188, 188
};

static tegra_dc_bl_output enterprise_bl_output_measured_a03 = {
	0, 1, 2, 3, 4, 5, 6, 7,
	8, 9, 10, 12, 13, 14, 15, 16,
	17, 19, 20, 21, 22, 22, 23, 24,
	25, 26, 27, 28, 29, 29, 30, 32,
	33, 34, 35, 36, 38, 39, 40, 42,
	43, 44, 46, 47, 49, 50, 51, 52,
	53, 54, 55, 56, 57, 58, 59, 60,
	61, 63, 64, 66, 67, 69, 70, 71,
	72, 73, 74, 75, 76, 77, 78, 79,
	80, 81, 82, 83, 84, 84, 85, 86,
	87, 88, 89, 90, 91, 92, 93, 94,
	95, 96, 97, 98, 99, 100, 101, 102,
	103, 104, 105, 106, 107, 108, 109, 110,
	110, 111, 112, 113, 113, 114, 115, 116,
	116, 117, 118, 118, 119, 120, 121, 122,
	123, 124, 125, 126, 127, 128, 129, 130,
	130, 131, 132, 133, 134, 135, 136, 137,
	138, 139, 140, 141, 142, 143, 144, 145,
	146, 147, 148, 149, 150, 151, 152, 153,
	154, 155, 156, 157, 158, 159, 160, 160,
	161, 162, 163, 163, 164, 165, 165, 166,
	167, 168, 168, 169, 170, 171, 172, 173,
	174, 175, 176, 176, 177, 178, 179, 180,
	181, 182, 183, 184, 185, 186, 187, 188,
	189, 190, 191, 191, 192, 193, 194, 194,
	195, 196, 197, 197, 198, 199, 199, 200,
	202, 203, 205, 206, 208, 209, 211, 212,
	213, 215, 216, 218, 219, 220, 221, 222,
	223, 224, 225, 226, 227, 228, 229, 230,
	231, 232, 233, 234, 235, 236, 237, 238,
	239, 240, 241, 243, 244, 245, 247, 248,
	250, 251, 251, 252, 253, 254, 254, 255,
};

static tegra_dc_bl_output tai_bl_output_measured = {
	0, 1, 2, 4, 5, 6, 8, 9,
	10, 12, 13, 14, 15, 16, 16, 17,
	18, 19, 20, 20, 21, 22, 24, 25,
	26, 27, 28, 29, 30, 31, 33, 34,
	35, 36, 37, 38, 39, 41, 42, 43,
	44, 45, 46, 46, 47, 48, 49, 50,
	50, 51, 52, 53, 53, 54, 55, 55,
	56, 57, 57, 58, 58, 59, 60, 61,
	62, 63, 64, 65, 65, 66, 67, 68,
	68, 69, 70, 70, 71, 72, 73, 73,
	74, 75, 76, 77, 77, 78, 79, 80,
	81, 82, 83, 84, 85, 86, 87, 87,
	88, 89, 90, 91, 92, 93, 94, 94,
	95, 95, 96, 97, 97, 98, 99, 99,
	100, 101, 101, 102, 103, 103, 104, 105,
	105, 106, 107, 108, 108, 109, 110, 111,
	111, 112, 113, 114, 115, 115, 116, 117,
	118, 119, 120, 121, 121, 122, 123, 124,
	125, 126, 126, 127, 128, 129, 130, 131,
	132, 133, 134, 134, 135, 136, 137, 138,
	139, 140, 141, 143, 144, 145, 146, 147,
	148, 149, 151, 152, 153, 154, 155, 156,
	157, 158, 159, 160, 161, 163, 164, 165,
	166, 167, 169, 170, 171, 172, 173, 175,
	176, 177, 179, 180, 182, 183, 185, 186,
	187, 189, 190, 191, 193, 194, 195, 197,
	198, 199, 200, 202, 203, 204, 205, 207,
	208, 209, 210, 212, 213, 214, 215, 216,
	218, 219, 220, 221, 222, 223, 225, 226,
	227, 228, 229, 231, 232, 233, 234, 235,
	237, 238, 239, 241, 242, 244, 245, 246,
	248, 249, 250, 251, 252, 253, 254, 255,
};

static p_tegra_dc_bl_output bl_output;

static bool kernel_1st_panel_init = true;

static int enterprise_backlight_notify(struct device *unused, int brightness)
{
	int cur_sd_brightness = atomic_read(&sd_brightness);

	/* SD brightness is a percentage, 8-bit value. */
	brightness = (brightness * cur_sd_brightness) / 255;

	/* Apply any backlight response curve */
	if (brightness > 255)
		pr_info("Error: Brightness > 255!\n");
	else
		brightness = bl_output[brightness];

	return brightness;
}

static int enterprise_disp1_check_fb(struct device *dev, struct fb_info *info);

static struct platform_pwm_backlight_data external_pwm_disp1_backlight_data = {
	.pwm_id		= 3,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 1000000,
	.notify		= enterprise_backlight_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= enterprise_disp1_check_fb,
};

#if IS_EXTERNAL_PWM
static struct platform_pwm_backlight_data enterprise_disp1_backlight_data = {
	.pwm_id		= 3,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 1000000,
	.notify		= enterprise_backlight_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= enterprise_disp1_check_fb,
};
#else
/*
 * In case which_pwm is TEGRA_PWM_PM0,
 * gpio_conf_to_sfio should be TEGRA_GPIO_PW0: set LCD_CS1_N pin to SFIO
 * In case which_pwm is TEGRA_PWM_PM1,
 * gpio_conf_to_sfio should be TEGRA_GPIO_PW1: set LCD_M1 pin to SFIO
 */
static struct platform_tegra_pwm_backlight_data enterprise_disp1_backlight_data = {
	.which_dc		= 0,
	.which_pwm		= TEGRA_PWM_PM1,
	.gpio_conf_to_sfio	= TEGRA_GPIO_PW1,
	.max_brightness		= 255,
	.dft_brightness		= 224,
	.notify		= enterprise_backlight_notify,
	.period			= 0xFF,
	.clk_div		= 0x3FF,
	.clk_select		= 0,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb	= enterprise_disp1_check_fb,
};
#endif


static struct platform_device enterprise_disp1_backlight_device = {
#if IS_EXTERNAL_PWM
	.name	= "pwm-backlight",
#else
	.name	= "tegra-pwm-bl",
#endif
	.id	= -1,
	.dev	= {
		.platform_data = &enterprise_disp1_backlight_data,
	},
};

static struct platform_device external_pwm_disp1_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &external_pwm_disp1_backlight_data,
	},
};
#ifdef CONFIG_TEGRA_DC
static int enterprise_hdmi_vddio_enable(struct device *dev)
{
	int ret;
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	if (!enterprise_hdmi_vddio) {
		enterprise_hdmi_vddio = regulator_get(dev, "hdmi_5v0");
		if (IS_ERR_OR_NULL(enterprise_hdmi_vddio)) {
			ret = PTR_ERR(enterprise_hdmi_vddio);
			pr_err("hdmi: couldn't get regulator hdmi_5v0\n");
			enterprise_hdmi_vddio = NULL;
			return ret;
		}
	}
	ret = regulator_enable(enterprise_hdmi_vddio);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator hdmi_5v0\n");
		regulator_put(enterprise_hdmi_vddio);
		enterprise_hdmi_vddio = NULL;
		return ret;
	}
	if (board_info.board_id == BOARD_E1239) {
		ret = gpio_request(TEGRA_GPIO_PM4, "en_hdmi_buffers");
		if (ret < 0) {
			pr_err("%s: gpio_request failed %d\n", __func__, ret);
			return ret;
		}

		ret = gpio_direction_output(TEGRA_GPIO_PM4, 1);
		if (ret < 0) {
			pr_err("%s: gpio_direction_ouput failed %d\n",
				__func__, ret);
			gpio_free(TEGRA_GPIO_PM4);
			return ret;
		}
	}

	return ret;
}

static int enterprise_hdmi_vddio_disable(void)
{
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	if (enterprise_hdmi_vddio) {
		regulator_disable(enterprise_hdmi_vddio);
		regulator_put(enterprise_hdmi_vddio);
		enterprise_hdmi_vddio = NULL;
	}
	if (board_info.board_id == BOARD_E1239) {
		gpio_set_value(TEGRA_GPIO_PM4, 0);
		gpio_free(TEGRA_GPIO_PM4);
	}
	return 0;
}

static int enterprise_hdmi_enable(struct device *dev)
{
	int ret;
	if (!enterprise_hdmi_reg) {
		enterprise_hdmi_reg = regulator_get(dev, "avdd_hdmi");
		if (IS_ERR_OR_NULL(enterprise_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			enterprise_hdmi_reg = NULL;
			return PTR_ERR(enterprise_hdmi_reg);
		}
	}
	ret = regulator_enable(enterprise_hdmi_reg);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi\n");
		return ret;
	}
	if (!enterprise_hdmi_pll) {
		enterprise_hdmi_pll = regulator_get(dev, "avdd_hdmi_pll");
		if (IS_ERR_OR_NULL(enterprise_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			enterprise_hdmi_pll = NULL;
			regulator_put(enterprise_hdmi_reg);
			enterprise_hdmi_reg = NULL;
			return PTR_ERR(enterprise_hdmi_pll);
		}
	}
	ret = regulator_enable(enterprise_hdmi_pll);
	if (ret < 0) {
		pr_err("hdmi: couldn't enable regulator avdd_hdmi_pll\n");
		return ret;
	}
	return 0;
}

static int enterprise_hdmi_disable(void)
{

	regulator_disable(enterprise_hdmi_reg);
	regulator_put(enterprise_hdmi_reg);
	enterprise_hdmi_reg = NULL;

	regulator_disable(enterprise_hdmi_pll);
	regulator_put(enterprise_hdmi_pll);
	enterprise_hdmi_pll = NULL;

	return 0;
}
static struct resource enterprise_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.start	= 0,	/* Filled in by enterprise_panel_init() */
		.end	= 0,	/* Filled in by enterprise_panel_init() */
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "dsi_regs",
		.start	= TEGRA_DSI_BASE,
		.end	= TEGRA_DSI_BASE + TEGRA_DSI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct resource enterprise_disp2_resources[] = {
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
		.flags	= IORESOURCE_MEM,
		.start	= 0,
		.end	= 0,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct tegra_dc_sd_settings enterprise_sd_settings = {
	.enable = 1, /* Normal mode operation */
	.use_auto_pwm = false,
	.hw_update_delay = 0,
	.bin_width = -1,
	.aggressiveness = 1,
	.phase_in_adjustments = true,
	.use_vid_luma = false,
	/* Default video coefficients */
	.coeff = {5, 9, 2},
	.fc = {0, 0},
	/* Immediate backlight changes */
	.blp = {1024, 255},
	/* Gammas: R: 2.2 G: 2.2 B: 2.2 */
	/* Default BL TF */
	.bltf = {
			{
				{57, 65, 74, 83},
				{93, 103, 114, 126},
				{138, 151, 165, 179},
				{194, 209, 225, 242},
			},
			{
				{58, 66, 75, 84},
				{94, 105, 116, 127},
				{140, 153, 166, 181},
				{196, 211, 227, 244},
			},
			{
				{60, 68, 77, 87},
				{97, 107, 119, 130},
				{143, 156, 170, 184},
				{199, 215, 231, 248},
			},
			{
				{64, 73, 82, 91},
				{102, 113, 124, 137},
				{149, 163, 177, 192},
				{207, 223, 240, 255},
			},
		},
	/* Default LUT */
	.lut = {
			{
				{250, 250, 250},
				{194, 194, 194},
				{149, 149, 149},
				{113, 113, 113},
				{82, 82, 82},
				{56, 56, 56},
				{34, 34, 34},
				{15, 15, 15},
				{0, 0, 0},
			},
			{
				{246, 246, 246},
				{191, 191, 191},
				{147, 147, 147},
				{111, 111, 111},
				{80, 80, 80},
				{55, 55, 55},
				{33, 33, 33},
				{14, 14, 14},
				{0, 0, 0},
			},
			{
				{239, 239, 239},
				{185, 185, 185},
				{142, 142, 142},
				{107, 107, 107},
				{77, 77, 77},
				{52, 52, 52},
				{30, 30, 30},
				{12, 12, 12},
				{0, 0, 0},
			},
			{
				{224, 224, 224},
				{173, 173, 173},
				{133, 133, 133},
				{99, 99, 99},
				{70, 70, 70},
				{46, 46, 46},
				{25, 25, 25},
				{7, 7, 7},
				{0, 0, 0},
			},
		},
	.sd_brightness = &sd_brightness,
#if IS_EXTERNAL_PWM
	.bl_device_name = "pwm-backlight",
#else
	.bl_device_name = "tegra-pwm-bl",
#endif
};

static struct tegra_fb_data enterprise_hdmi_fb_data = {
	.win		= 0,
	.xres		= 1366,
	.yres		= 768,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_out enterprise_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,
	.parent_clk	= "pll_d2_out0",

	.dcc_bus	= 3,
	.hotplug_gpio	= enterprise_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(148500),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= enterprise_hdmi_enable,
	.disable	= enterprise_hdmi_disable,
	.postsuspend	= enterprise_hdmi_vddio_disable,
	.hotplug_init	= enterprise_hdmi_vddio_enable,
};

static struct tegra_dc_platform_data enterprise_disp2_pdata = {
	.flags		= 0,
	.default_out	= &enterprise_disp2_out,
	.fb		= &enterprise_hdmi_fb_data,
	.emc_clk_rate	= 300000000,
};

static int avdd_dsi_csi_rail_enable(void)
{
	int ret;

	if (dsi_regulator_status == true)
		return 0;

	if (enterprise_dsi_reg == NULL) {
		enterprise_dsi_reg = regulator_get(NULL, "avdd_dsi_csi");
		if (IS_ERR_OR_NULL(enterprise_dsi_reg)) {
			pr_err("dsi: Could not get regulator avdd_dsi_csi\n");
			enterprise_dsi_reg = NULL;
			return PTR_ERR(enterprise_dsi_reg);
		}
	}
	ret = regulator_enable(enterprise_dsi_reg);
	if (ret < 0) {
		pr_err("DSI regulator avdd_dsi_csi could not be enabled\n");
		return ret;
	}
	dsi_regulator_status = true;
	return 0;
}

static int avdd_dsi_csi_rail_disable(void)
{
	int ret;

	if (dsi_regulator_status == false)
		return 0;

	if (enterprise_dsi_reg == NULL) {
		pr_warn("%s: unbalanced disable\n", __func__);
		return -EIO;
	}

	ret = regulator_disable(enterprise_dsi_reg);
	if (ret < 0) {
		pr_err("DSI regulator avdd_dsi_csi cannot be disabled\n");
		return ret;
	}
	dsi_regulator_status = false;
	return 0;
}

static int enterprise_dsi_panel_enable(struct device *dev)
{
	int ret;
	struct board_info board_info;

	tegra_get_board_info(&board_info);

	ret = avdd_dsi_csi_rail_enable();
	if (ret)
		return ret;


#if DSI_PANEL_RESET
	if ((board_info.fab >= BOARD_FAB_A03) ||
		(board_info.board_id == BOARD_E1239)) {
		if (enterprise_lcd_reg == NULL) {
			enterprise_lcd_reg = regulator_get(dev, "lcd_vddio_en");
			if (IS_ERR_OR_NULL(enterprise_lcd_reg)) {
				pr_err("Could not get regulator lcd_vddio_en\n");
				ret = PTR_ERR(enterprise_lcd_reg);
				enterprise_lcd_reg = NULL;
				return ret;
			}
		}
		if (enterprise_lcd_reg != NULL) {
			ret = regulator_enable(enterprise_lcd_reg);
			if (ret < 0) {
				pr_err("Could not enable lcd_vddio_en\n");
				return ret;
			}
		}
	}

	if (kernel_1st_panel_init == true) {
		ret = gpio_request(enterprise_dsi_panel_reset, "panel reset");
		if (ret < 0)
			return ret;
		kernel_1st_panel_init = false;
	} else {
		ret = gpio_direction_output(enterprise_dsi_panel_reset, 0);
		if (ret < 0) {
			pr_err("%s: gpio_direction_ouput failed %d\n",
				__func__, ret);
			gpio_free(enterprise_dsi_panel_reset);
			return ret;
		}
		gpio_set_value(enterprise_dsi_panel_reset, 0);
		udelay(2000);
		gpio_set_value(enterprise_dsi_panel_reset, 1);
		mdelay(20);
	}
#endif

	return ret;
}

static int enterprise_dsi_panel_disable(void)
{
#if DSI_PANEL_RESET
	int ret;
#endif
	if (enterprise_lcd_reg != NULL)
		regulator_disable(enterprise_lcd_reg);

#if DSI_PANEL_RESET
	ret = gpio_direction_output(enterprise_dsi_panel_reset, 0);
	if (ret < 0) {
		pr_err("%s: gpio_direction_ouput failed %d\n",
			__func__, ret);
		gpio_free(enterprise_dsi_panel_reset);
		return ret;
	}
#endif
	return 0;
}
#endif

static void enterprise_stereo_set_mode(int mode)
{
	switch (mode) {
	case TEGRA_DC_STEREO_MODE_2D:
		gpio_set_value(TEGRA_GPIO_PH1, ENTERPRISE_STEREO_2D);
		break;
	case TEGRA_DC_STEREO_MODE_3D:
		gpio_set_value(TEGRA_GPIO_PH1, ENTERPRISE_STEREO_3D);
		break;
	}
}

static void enterprise_stereo_set_orientation(int mode)
{
	switch (mode) {
	case TEGRA_DC_STEREO_LANDSCAPE:
		gpio_set_value(TEGRA_GPIO_PH2, ENTERPRISE_STEREO_LANDSCAPE);
		break;
	case TEGRA_DC_STEREO_PORTRAIT:
		gpio_set_value(TEGRA_GPIO_PH2, ENTERPRISE_STEREO_PORTRAIT);
		break;
	}
}

#ifdef CONFIG_TEGRA_DC
static int enterprise_dsi_panel_postsuspend(void)
{
	/* Disable enterprise dsi rail */
	return avdd_dsi_csi_rail_disable();
}
#endif

static struct tegra_dsi_cmd dsi_init_cmd[]= {
	DSI_CMD_SHORT(0x05, 0x11, 0x00),
	DSI_DLY_MS(20),
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};

static struct tegra_dsi_cmd dsi_early_suspend_cmd[] = {
	DSI_CMD_SHORT(0x05, 0x28, 0x00),
	DSI_DLY_MS(20),
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x05, 0x34, 0x00),
#endif
};

static struct tegra_dsi_cmd dsi_late_resume_cmd[] = {
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x15, 0x35, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x29, 0x00),
	DSI_DLY_MS(20),
};

static struct tegra_dsi_cmd dsi_suspend_cmd[] = {
	DSI_CMD_SHORT(0x05, 0x28, 0x00),
	DSI_DLY_MS(20),
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	DSI_CMD_SHORT(0x05, 0x34, 0x00),
#endif
	DSI_CMD_SHORT(0x05, 0x10, 0x00),
	DSI_DLY_MS(5),
};

struct tegra_dsi_out enterprise_dsi = {
	.n_data_lanes = 2,
	.pixel_format = TEGRA_DSI_PIXEL_FORMAT_24BIT_P,
#if(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	/* For one-shot mode, actual refresh rate is decided by the
	 * frequency of TE signal. Although the frequency of TE is
	 * expected running at rated_refresh_rate (typically 60Hz),
	 * it may vary. Mismatch between freq of DC and TE signal
	 * would cause frame drop. We increase refresh_rate to the
	 * value larger than maximum TE frequency to avoid missing
	 * any TE signal. The value of refresh_rate is also used to
	 * calculate the pixel clock.
	 */
	.refresh_rate = 66,
	.rated_refresh_rate = 60,
#else
	.refresh_rate = 60,
#endif
	.virtual_channel = TEGRA_DSI_VIRTUAL_CHANNEL_0,

	.panel_has_frame_buffer = true,
	.dsi_instance = 0,

	.panel_reset = DSI_PANEL_RESET,
	.power_saving_suspend = true,
	.n_init_cmd = ARRAY_SIZE(dsi_init_cmd),
	.dsi_init_cmd = dsi_init_cmd,

	.n_early_suspend_cmd = ARRAY_SIZE(dsi_early_suspend_cmd),
	.dsi_early_suspend_cmd = dsi_early_suspend_cmd,

	.n_late_resume_cmd = ARRAY_SIZE(dsi_late_resume_cmd),
	.dsi_late_resume_cmd = dsi_late_resume_cmd,

	.n_suspend_cmd = ARRAY_SIZE(dsi_suspend_cmd),
	.dsi_suspend_cmd = dsi_suspend_cmd,

	.video_data_type = TEGRA_DSI_VIDEO_TYPE_COMMAND_MODE,
	.lp_cmd_mode_freq_khz = 20000,

	/* TODO: Get the vender recommended freq */
	.lp_read_cmd_mode_freq_khz = 200000,
};

static struct tegra_stereo_out enterprise_stereo = {
	.set_mode		= &enterprise_stereo_set_mode,
	.set_orientation	= &enterprise_stereo_set_orientation,
};

#ifdef CONFIG_TEGRA_DC
static struct tegra_dc_mode enterprise_dsi_modes[] = {
	{
#if (DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
		.pclk = 39446000,
#else
		.pclk = 35860000,
#endif
		.h_ref_to_sync = 4,
		.v_ref_to_sync = 1,
		.h_sync_width = 16,
		.v_sync_width = 1,
		.h_back_porch = 32,
		.v_back_porch = 1,
		.h_active = 540,
		.v_active = 960,
		.h_front_porch = 32,
		.v_front_porch = 2,
	},
};

static struct tegra_fb_data enterprise_dsi_fb_data = {
	.win		= 0,
	.xres		= 540,
	.yres		= 960,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

static struct tegra_dc_out enterprise_disp1_out = {
	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.sd_settings	= &enterprise_sd_settings,

	.flags		= DC_CTRL_MODE,

	.type		= TEGRA_DC_OUT_DSI,

	.modes		= enterprise_dsi_modes,
	.n_modes	= ARRAY_SIZE(enterprise_dsi_modes),

	.dsi		= &enterprise_dsi,
	.stereo		= &enterprise_stereo,

	.enable		= enterprise_dsi_panel_enable,
	.disable	= enterprise_dsi_panel_disable,
	.postsuspend	= enterprise_dsi_panel_postsuspend,

	.width		= 53,
	.height		= 95,
};
static struct tegra_dc_platform_data enterprise_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &enterprise_disp1_out,
	.emc_clk_rate	= 204000000,
	.fb		= &enterprise_dsi_fb_data,
};

static struct platform_device enterprise_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= enterprise_disp1_resources,
	.num_resources	= ARRAY_SIZE(enterprise_disp1_resources),
	.dev = {
		.platform_data = &enterprise_disp1_pdata,
	},
};

static int enterprise_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &enterprise_disp1_device.dev;
}

static struct platform_device enterprise_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= enterprise_disp2_resources,
	.num_resources	= ARRAY_SIZE(enterprise_disp2_resources),
	.dev = {
		.platform_data = &enterprise_disp2_pdata,
	},
};
#endif

#if defined(CONFIG_TEGRA_NVMAP)
static struct nvmap_platform_carveout enterprise_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.base		= 0,	/* Filled in by enterprise_panel_init() */
		.size		= 0,	/* Filled in by enterprise_panel_init() */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data enterprise_nvmap_data = {
	.carveouts	= enterprise_carveouts,
	.nr_carveouts	= ARRAY_SIZE(enterprise_carveouts),
};

static struct platform_device enterprise_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &enterprise_nvmap_data,
	},
};
#endif

static struct platform_device *enterprise_gfx_devices[] __initdata = {
#if defined(CONFIG_TEGRA_NVMAP)
	&enterprise_nvmap_device,
#endif
#if IS_EXTERNAL_PWM
	&tegra_pwfm3_device,
#else
	&tegra_pwfm0_device,
#endif
};

static struct platform_device *external_pwm_gfx_devices[] __initdata = {
#if defined(CONFIG_TEGRA_NVMAP)
	&enterprise_nvmap_device,
#endif
	&tegra_pwfm3_device,
};

static struct platform_device *enterprise_bl_devices[]  = {
	&enterprise_disp1_backlight_device,
};

int __init enterprise_panel_init(void)
{
	int err;
	struct resource __maybe_unused *res;
	struct board_info board_info;
	struct platform_device *phost1x;

	tegra_get_board_info(&board_info);

	BUILD_BUG_ON(ARRAY_SIZE(enterprise_bl_output_measured_a03) != 256);
	BUILD_BUG_ON(ARRAY_SIZE(enterprise_bl_output_measured_a02) != 256);

	if (board_info.board_id != BOARD_E1239) {
		if (board_info.fab >= BOARD_FAB_A03) {
#if !(IS_EXTERNAL_PWM)
			enterprise_disp1_backlight_data.clk_div = 0x1D;
#endif
			bl_output = enterprise_bl_output_measured_a03;
		} else
			bl_output = enterprise_bl_output_measured_a02;
	} else {
		enterprise_bl_devices[0] = &external_pwm_disp1_backlight_device;
		bl_output = tai_bl_output_measured;
	}
	enterprise_dsi.chip_id = tegra_get_chipid();
	enterprise_dsi.chip_rev = tegra_revision;

#if defined(CONFIG_TEGRA_NVMAP)
	enterprise_carveouts[1].base = tegra_carveout_start;
	enterprise_carveouts[1].size = tegra_carveout_size;
#endif

	err = gpio_request(enterprise_hdmi_hpd, "hdmi_hpd");
	if (err < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, err);
		return err;
	}
	err = gpio_direction_input(enterprise_hdmi_hpd);
	if (err < 0) {
		pr_err("%s: gpio_direction_input failed %d\n",
			__func__, err);
		gpio_free(enterprise_hdmi_hpd);
		return err;
	}

	if (board_info.board_id != BOARD_E1239) {
		err = gpio_request(enterprise_lcd_2d_3d, "lcd_2d_3d");
		if (err < 0) {
			pr_err("%s: gpio_request failed %d\n", __func__, err);
			return err;
		}
		err = gpio_direction_output(enterprise_lcd_2d_3d, 0);
		if (err < 0) {
			pr_err("%s: gpio_direction_ouput failed %d\n",
				__func__, err);
			gpio_free(enterprise_lcd_2d_3d);
			return err;
		}
		enterprise_stereo_set_mode(enterprise_stereo.mode_2d_3d);

		err = gpio_request(enterprise_lcd_swp_pl, "lcd_swp_pl");
		if (err < 0) {
			pr_err("%s: gpio_request failed %d\n", __func__, err);
			return err;
		}
		err = gpio_direction_output(enterprise_lcd_swp_pl, 0);
		if (err < 0) {
			pr_err("%s: gpio_direction_ouput failed %d\n",
				__func__, err);
			gpio_free(enterprise_lcd_swp_pl);
			return err;
		}
		enterprise_stereo_set_orientation(
						enterprise_stereo.orientation);
#if IS_EXTERNAL_PWM
		err = gpio_request(enterprise_bl_pwm, "bl_pwm");
		if (err < 0) {
			pr_err("%s: gpio_request failed %d\n", __func__, err);
			return err;
		}
		gpio_free(enterprise_bl_pwm);
#endif
	} else {
		/* External pwm is used but do not use IS_EXTERNAL_PWM
		compiler switch for TAI */
		err = gpio_request(enterprise_bl_pwm, "bl_pwm");
		if (err < 0) {
			pr_err("%s: gpio_request failed %d\n", __func__, err);
			return err;
		}
		gpio_free(enterprise_bl_pwm);
	}

#if !(DC_CTRL_MODE & TEGRA_DC_OUT_ONE_SHOT_MODE)
	err = gpio_request(enterprise_lcd_swp_pl, "lcd_te");
	if (err < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, err);
		return err;
	}
	err = gpio_direction_input(enterprise_lcd_te);
	if (err < 0) {
		pr_err("%s: gpio_direction_input failed %d\n",
			__func__, err);
		gpio_free(enterprise_lcd_te);
		return err;
	}
#endif

	if (board_info.board_id != BOARD_E1239)
		err = platform_add_devices(enterprise_gfx_devices,
			ARRAY_SIZE(enterprise_gfx_devices));
	else
		err = platform_add_devices(external_pwm_gfx_devices,
			ARRAY_SIZE(external_pwm_gfx_devices));

#ifdef CONFIG_TEGRA_GRHOST
	phost1x = tegra3_register_host1x_devices();
	if (!phost1x)
		return -EINVAL;
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = platform_get_resource_byname(&enterprise_disp1_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;
#endif

	/* Copy the bootloader fb to the fb. */
	__tegra_move_framebuffer(&enterprise_nvmap_device,
		tegra_fb_start, tegra_bootloader_fb_start,
		min(tegra_fb_size, tegra_bootloader_fb_size));

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	if (!err) {
		enterprise_disp1_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&enterprise_disp1_device);
	}

	res = platform_get_resource_byname(&enterprise_disp2_device,
					 IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb2_start;
	res->end = tegra_fb2_start + tegra_fb2_size - 1;
	if (!err) {
		enterprise_disp2_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&enterprise_disp2_device);
	}
#endif

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_NVAVP)
	if (!err) {
		nvavp_device.dev.parent = &phost1x->dev;
		err = platform_device_register(&nvavp_device);
	}
#endif

	if (!err)
		err = platform_add_devices(enterprise_bl_devices,
				ARRAY_SIZE(enterprise_bl_devices));

	return err;
}
