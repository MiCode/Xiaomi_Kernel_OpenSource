// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

static atomic_t current_backlight;
#define ENABLE_DSC 1
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos;
	struct gpio_desc *bias_neg;
	bool prepared;
	bool enabled;

	int error;
};

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...) \
({\
	static const u8 d[] = { seq };\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret,
			 cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = { 0 };
	static int ret;

	pr_info("%s+\n", __func__);

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		pr_info("%s  0x%08x\n", __func__, buffer[0] | (buffer[1] << 8));
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	char *addr;

	if (ctx->error < 0)
		return;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static void lcm_panel_init(struct lcm *ctx)
{
	char bl_tb[] = {0x51, 0x07, 0xff};
	unsigned int level = 0;

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	/* Switch PAGE0 */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	/* Voltage */
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x06);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x2B, 0x06, 0x33);
	/* IDLE */
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x2B, 0x23, 0x33);
	/* ELVSS_SWIRE 1~5 for 144 */
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x06, 0x06, 0x06, 0x06, 0x06);
	/* CMD Mode */
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x76, 0xF3, 0xC1);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x96);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x04, 0xB0);
	lcm_dcs_write_seq_static(ctx, 0xC9, 0x84);

	/* Switch Page4 */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x05, 0x0F, 0x1F, 0x3E, 0x7C);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x06);
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x00, 0x08, 0x00, 0x3C,
			0x01, 0x48, 0x07, 0xFF, 0x0F, 0xFF);
	/* F_R_EN_ON */
	lcm_dcs_write_seq_static(ctx, 0xC2, 0x14);
	/* SPI Speed */
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x02);
	/* FLASH DMR SEL */
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x40);
	/* DMR addr && size for reload */
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x00, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x09, 0xE3, 0x40);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x07);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x09, 0xE4, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x09, 0xE3, 0x40);
	/* DMR Setting */
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x86);
	lcm_dcs_write_seq_static(ctx, 0xD0, 0x00, 0x00, 0x00, 0x10);

	/* SPR off */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xDE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x09);
	//lcm_dcs_write_seq_static(ctx, 0xDE, 0x10, 0x34, 0x25, 0x10, 0x34, 0x25);//MTK CHANGE COLOR
	lcm_dcs_write_seq_static(ctx, 0xDE, 0x10, 0x34, 0x25, 0x30, 0x14, 0x25);//nvt CHANGE COLOR

	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x07);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x24);

		/* Switch Page8 */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x08);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x8E, 0xFF);
	//Vporth Setting ??
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x00, 0x14, 0x00, 0x1C);
	//DSC setting
	lcm_dcs_write_seq_static(ctx, 0x03, 0x01);
#if ENABLE_DSC
	lcm_dcs_write_seq_static(ctx, 0x90, 0x91);
	/* DSC slice high = 60 */
	lcm_dcs_write_seq_static(ctx, 0x91, 0x80, 0xF0, 0x00, 0x3C, 0xF1,
		0x55, 0x02, 0x28, 0x04, 0xA1, 0x00, 0x5A, 0x02, 0x09, 0x04,
		0xC4, 0x0B, 0xD0);
#else
	lcm_dcs_write_seq_static(ctx, 0x90, 0x00);
#endif
	lcm_dcs_write_seq_static(ctx, 0x2C);

	//AC/DC Dimming
	lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0xFF, 0x0F, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x28);

	//TE on
	lcm_dcs_write_seq_static(ctx, 0X35, 0X01);//TE Enable

	//Gram Address
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x09, 0x5F);

	//FrameRate 60Hz:0x01  120HZ:0x02
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x01);

	//backlight
	level = atomic_read(&current_backlight);
	bl_tb[1] = (level >> 8) & 0x7;
	bl_tb[2] = level & 0xFF;
	lcm_dcs_write(ctx, bl_tb, ARRAY_SIZE(bl_tb));

	/* Sleep Out */
	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(120);
	/* Display On */
	lcm_dcs_write_seq_static(ctx, 0x29);

	pr_info("%s-\n", __func__);
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	lcm_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(200);
	lcm_dcs_write_seq_static(ctx, 0x4f, 0x01);
	msleep(120);
	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	// lcd reset H -> L -> L
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10000, 10001);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	// end
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	pr_info("%s-\n", __func__);
	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

#define HFP (40)
#define HSA (10)
#define HBP (20)
#define HACT (1080)
#define VFP (20)
#define VSA (2)
#define VBP (8)
#define VACT (2400)
#define PLL_CLOCK (410)

static const struct drm_display_mode default_mode = {
#if ENABLE_DSC
	.clock		= 335340,//192378,
#else
	.clock		= 265628,
#endif
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,//1150
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,//2430:disabel dsc 4900 enable dsc
};

static const struct drm_display_mode switch_mode_60hz = {
	.clock = 167670,
	.hdisplay	= HACT,
	.hsync_start	= HACT + HFP,
	.hsync_end	= HACT + HFP + HSA,
	.htotal		= HACT + HFP + HSA + HBP,//1150
	.vdisplay	= VACT,
	.vsync_start	= VACT + VFP,
	.vsync_end	= VACT + VFP + VSA,
	.vtotal		= VACT + VFP + VSA + VBP,//2430
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.data_rate = PLL_CLOCK * 2,
	.pll_clk = PLL_CLOCK,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.cm_params = {
		.enable = 1,
		.relay = 1,
		.cm_c00 = 230*4,//0x62 0xe6
		.cm_c01 = 18*4,//0x64 0x12
		.cm_c02 = 8*4,
		.cm_c10 = 1*4,
		.cm_c11 = 239*4,//0x6a 0xef
		.cm_c12 = 5*4,
		.cm_c20 = 0,
		.cm_c21 = 1*4,
		.cm_c22 = 218*4,
		.cm_gray_en = 1,
	},
	.spr_params = {
		.enable = 1,
		.relay = 0,
		.postalign_en = 1,
		.bypass_dither = 1,
		.custom_header = 11,//0x2C for default set
		.spr_format_type = MTK_PANEL_RGBG_BGRG_TYPE,
		//.rg_xy_swap = 1,
		.spr_color_params[0] = {SPR_WEIGHT_SET, 36,
			{64, 64, 128, 128, 0, 0, 0, 0,
			128, 128, 64, 64, 0, 0, 0, 0,
			0, 0, 0, 0, 128, 128, 64, 64,
			64, 64, 128, 128, 0, 0, 0, 0,
			0, 0, 0, 0}},
		.spr_color_params[1] = {SPR_BORDER_SET, 80,
			{0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 1, 0, 0, 0, 0, 0, 0,
			0, 4, 0, 0, 0, 0, 0, 0,
			64, 128, 64, 128, 64, 128, 64, 128}},
		.spr_color_params[2] = {SPR_SPE_SET, 60,
			{0x10, 101, 0x10, 157, 0x10, 229, 0x11, 121,
			0x11, 121, 0x11, 121, 0x10, 5, 0x10, 5,
			0x10, 120, 0x10, 120, 0x10, 5, 0x10, 157,
			0x10, 101, 0x10, 229, 0x11, 29, 0x11, 121,
			0x11, 121, 0x11, 121, 0x11, 121, 0x11, 121,
			0x11, 121, 0x11, 121, 0x1, 121, 0, 0,
			0, 0, 0, 0, 0x1, 121, 0, 0,
			0, 0, 0, 0}},
	},
	.spr_output_mode = MTK_PANEL_PACKED_SPR_8_BITS,

#if ENABLE_DSC
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 2,
		.slice_mode = 0,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 0,//spr enable no need convert rgb
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 0,//9,//16
		.bp_enable = 1,
		.bit_per_pixel = 256,//128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 60,//8,
		.slice_width = 1080,
		.chunk_size = 1080,
		.xmit_delay = 341,//512,
		.dec_delay = 552,//796,
		.scale_value = 10,//32,
		.increment_interval = 1185,//246,
		.decrement_interval = 90,//15,
		.line_bpg_offset = 15,//12,
		.nfl_bpg_offset = 521,//3511,
		.slice_bpg_offset = 1220,//1628,
		.initial_offset = 2048,//6144,
		.final_offset = 3024,//4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
#endif
};

static struct mtk_panel_params ext_params_60hz = {
	.data_rate = PLL_CLOCK * 2,
	.pll_clk = PLL_CLOCK,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.cm_params = {
		.enable = 1,
		.relay = 1,
		.cm_c00 = 230*4,//0x62 0xe6
		.cm_c01 = 18*4,//0x64 0x12
		.cm_c02 = 8*4,
		.cm_c10 = 1*4,
		.cm_c11 = 239*4,//0x6a 0xef
		.cm_c12 = 5*4,
		.cm_c20 = 0,
		.cm_c21 = 1*4,
		.cm_c22 = 218*4,
		.cm_gray_en = 1,
	},
	.spr_params = {
		.enable = 1,
		.relay = 0,
		.postalign_en = 1,
		.bypass_dither = 1,
		.custom_header = 11,//0x2C for default set
		.spr_format_type = MTK_PANEL_RGBG_BGRG_TYPE,
		//.rg_xy_swap = 1,
		.spr_color_params[0] = {SPR_WEIGHT_SET, 36,
			{0x0, 0x0, 0x88, 0x78, 0x48, 0xb8, 0x48, 0xb8,
			0x88, 0x78, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x48, 0xb8,
			0x88, 0x78, 0x0, 0x0, 0x0, 0x0, 0x88, 0x78, 0x48, 0xb8, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0}},
		.spr_color_params[1] = {SPR_BORDER_SET, 80, {0x0, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x40, 0x80, 0x80, 0x80, 0x80, 0x80, 0x60, 0x80}},
		.spr_color_params[2] = {SPR_SPE_SET, 60, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
			0x0, 0x0, 0x0, 0x0, 0xa0, 0x11, 0xa0, 0x11,
			0xa0, 0x11, 0xa0, 0x11, 0x98, 0x11, 0x98, 0x11,
			0x98, 0x11, 0x98, 0x11}},

	},
	.spr_output_mode = MTK_PANEL_PACKED_SPR_8_BITS,

#if ENABLE_DSC
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 2,
		.slice_mode = 0,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 0,//spr enable no need convert rgb
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 0,//9,//16
		.bp_enable = 1,
		.bit_per_pixel = 256,//128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 60,//8,
		.slice_width = 1080,
		.chunk_size = 1080,
		.xmit_delay = 341,//512,
		.dec_delay = 552,//796,
		.scale_value = 10,//32,
		.increment_interval = 1185,//246,
		.decrement_interval = 90,//15,
		.line_bpg_offset = 15,//12,
		.nfl_bpg_offset = 521,//3511,
		.slice_bpg_offset = 1220,//1628,
		.initial_offset = 2048,//6144,
		.final_offset = 3024,//4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
	},
#endif
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				 unsigned int level)
{
	char bl_tb[] = {0x51, 0x07, 0xff};

	bl_tb[1] = (level >> 8) & 0x7;
	bl_tb[2] = level & 0xFF;
	if (!cb)
		return -1;
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
	atomic_set(&current_backlight, level);
	pr_info("%s %d %d %d\n", __func__, level, bl_tb[1], bl_tb[2]);
	return 0;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);
	if (!m) {
		pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
		return ret;
	}
	if (drm_mode_vrefresh(m) == 120)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params_60hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct lcm *ctx = panel_to_lcm(panel);

		lcm_dcs_write_seq_static(ctx, 0x2F, 0x01);
	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);

	if (cur_mode == dst_mode)
		return ret;
	if (drm_mode_vrefresh(m) == 60) { /*switch to 60 */
		mode_switch_to_60(panel, stage);
	} else if (drm_mode_vrefresh(m) == 120) { /*switch to 120 */
		mode_switch_to_120(panel, stage);
	} else
		ret = 1;

	return ret;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
};
#endif

static int lcm_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;

	mode = drm_mode_duplicate(connector->dev, &switch_mode_60hz);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	mode_1 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);

	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s+\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info("%s+ skip probe due to not current lcm\n", __func__);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			 PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	atomic_set(&current_backlight, 4095);
	devm_gpiod_put(dev, ctx->reset_gpio);
	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif

	pr_info("%s- lcm,nt37701,cmd,120hz\n", __func__);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{
	    .compatible = "hx,nt37701c,cmd,spr,120hz",
	},
	{}
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-hx-nt37701-dphy-cmd-120hz",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("huaxing nt37701 AMOLED CMD SPR LCD Panel Driver");
MODULE_LICENSE("GPL v2");
