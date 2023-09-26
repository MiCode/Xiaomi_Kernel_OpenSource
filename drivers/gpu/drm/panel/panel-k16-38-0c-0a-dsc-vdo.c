/*
copyright (c) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern_k16.h"
#endif

#include "../mediatek/mi_disp/mi_dsi_panel_count.h"

#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#define DATA_RATE               1100
#define HACT                    1080
#define VACT                    2400

/* Porch Values Setting for Mode 0 Start */
#define MODE0_FPS               60
#define MODE0_HFP               98
#define MODE0_HSA               16
#define MODE0_HBP               58
#define MODE0_VFP               2452
#define MODE0_VSA               2
#define MODE0_VBP               10
/* Porch Values Setting for Mode 0 End */

/* Porch Values Setting for Mode 1 Start */
#define MODE1_FPS               120
#define MODE1_HFP               98
#define MODE1_HSA               16
#define MODE1_HBP               58
#define MODE1_VFP               20
#define MODE1_VSA               2
#define MODE1_VBP               10
/* Porch Values Setting for Mode 1 End */

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */

static char bl_tb0[] = {0x51, 0x3, 0xff};
static struct mtk_ddic_dsi_msg cmd_msg = {0};
static unsigned int last_backlight = 0;
static const char *panel_name = "panel_name=dsi_k16_38_0c_0a_dsc_vdo";

#define lcm_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
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
		dev_err(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
		ctx->error = ret;
	}

	return ret;
}

static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
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
		dev_err(ctx->dev, "error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static struct regulator *disp_vci;

static int lcm_panel_vci_regulator_init(struct device *dev)
{
       static int regulator_inited;
       int ret = 0;

       if (regulator_inited)
               return ret;

       /* please only get regulator once in a driver */
       disp_vci = regulator_get(dev, "vibr");
       if (IS_ERR(disp_vci)) { /* handle return value */
               ret = PTR_ERR(disp_vci);
               pr_err("get disp_vci fail, error: %d\n", ret);
               return ret;
       }

       regulator_inited = 1;
       return ret; /* must be 0 */

}

static int lcm_panel_vci_enable(struct device *dev)
{
       int ret = 0;
       int retval = 0;
       int status = 0;

       pr_err("%s +\n",__func__);

       lcm_panel_vci_regulator_init(dev);

       /* set voltage with min & max*/
       ret = regulator_set_voltage(disp_vci, 3000000, 3000000);
       if (ret < 0)
               pr_err("set voltage disp_vci fail, ret = %d\n", ret);
       retval |= ret;

       status = regulator_is_enabled(disp_vci);
       pr_err("%s regulator_is_enabled=%d\n",__func__,status);
       if(!status){
               /* enable regulator */
               ret = regulator_enable(disp_vci);
               if (ret < 0)
                       pr_err("enable regulator disp_vci fail, ret = %d\n", ret);
               retval |= ret;
       }

       pr_err("%s -\n",__func__);
       return retval;
}
static unsigned int start_up = 1;
static int lcm_panel_vci_disable(struct device *dev)
{
       int ret = 0;
       int retval = 0;
       //int status = 0;
       pr_err("%s +\n",__func__);

       lcm_panel_vci_regulator_init(dev);

       //status = regulator_is_enabled(disp_vci);
       //pr_err("%s regulator_is_enabled=%d\n",__func__,status);
       //if(!status){
       if(start_up) {
               pr_err("%s enable regulator\n",__func__);
               /* enable regulator */
               ret = regulator_enable(disp_vci);
               if (ret < 0)
                       pr_err("enable regulator disp_vci fail, ret = %d\n", ret);
               retval |= ret;

               start_up = 0;
       }
       //}

       ret = regulator_disable(disp_vci);
       if (ret < 0)
               pr_err("disable regulator disp_vci fail, ret = %d\n", ret);
       retval |= ret;

       pr_err("%s -\n",__func__);

       return retval;
}


static void lcm_panel_init(struct lcm *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	msleep(12);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(2);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(12);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	/* sleep out */
	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(20);

	/* Dimming Control */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x01, 0x31); // 11 Bit Dim On
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x09, 0x30, 0x95, 0x46, 0xE9);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);

	/* Compression Enable */
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);
	/* PPS Setting */
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60, 0x04,
				      0x38, 0x00, 0x28, 0x02, 0x1C, 0x02, 0x1C, 0x02, 0x00, 0x02,
				      0x0E, 0x00, 0x20, 0x03, 0xDD, 0x00, 0x07, 0x00, 0x0C, 0x02,
				      0x77, 0x02, 0x8B, 0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20,
				      0x00, 0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38, 0x46,
				      0x54 ,0x62, 0x69, 0x70, 0x77, 0x79, 0x7B, 0x7D, 0x7E, 0x01,
				      0x02, 0x01, 0x00, 0x09, 0x40, 0x09, 0xBE, 0x19, 0xFC, 0x19,
				      0xFA, 0x19, 0xF8, 0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A,
				      0xF6, 0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4, 0x00);

	/* Level2 Access Enable */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);

	/* 60 Hz */
	lcm_dcs_write_seq_static(ctx, 0x60, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);

	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x15, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x28, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x3B, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x98);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x11, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0xEE);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x18, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x1C);

	/* Flat Gamma Control */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x88, 0xB1);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x27, 0x0D, 0xFB, 0x0F, 0xA4, 0x86, 0x01, 0xFF, 0x10,
				      0x33, 0xFF, 0x10, 0xFF, 0x35, 0x34, 0x5A, 0x0A, 0x0A, 0x0A,
				      0x29, 0x29, 0x29, 0x37, 0x37, 0x37, 0x41, 0x41, 0x41, 0x45,
				      0x45, 0x45, 0x1B, 0x2F, 0xDA, 0x18, 0x74, 0x80, 0x00, 0x00, 0x22); /*Flat mode off*/

	/* VINT Voltage control */
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x11, 0xFE);
	lcm_dcs_write_seq_static(ctx, 0xFE, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);

	/* Dimming Setting */
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0D, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x20); // Dimming Speed: 32 Frames
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0C, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x30); // Dimming On
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x0B);

	/* ESD Setting */
	lcm_dcs_write_seq_static(ctx, 0xFC, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0x9B);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x05, 0xE1);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0xF9, 0xFC);
	lcm_dcs_write_seq_static(ctx, 0xED, 0x01, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x06, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x1F);

	/* Level2 Access Disable */
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
	lcm_dcs_write_seq_static(ctx, 0xFC, 0xA5, 0xA5);

	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);

	msleep(100);

	/* Display on */
	lcm_dcs_write_seq_static(ctx, 0x29, 0x00);
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

	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00);/*close oled backlight */
	if (ctx->hbm_en)
		lcm_dcs_write_seq_static(ctx, 0x53, 0x20, 0x00);
	msleep(20);
	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;

	pr_info("%s+\n", __func__);
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(10);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	lcm_panel_vci_disable(ctx->dev);

	ctx->vddi_enable = devm_gpiod_get(ctx->dev, "vddi-enable", GPIOD_OUT_HIGH);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddi_enable, 0);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddi_enable);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->error = 0;
	ctx->prepared = false;
	panel->panel_initialized = false;

	/* add for display fps cpunt */
	dsi_panel_fps_count(ctx, 0, 0);
	/* add for display state count*/
	if (ctx->mi_count.panel_active_count_enable)
		dsi_panel_state_count(ctx, 0);
	/* add for display hbm count */
	dsi_panel_HBM_count(ctx, 0, 1);

	pr_info("%s-\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	ctx->vddi_enable = devm_gpiod_get(ctx->dev, "vddi-enable", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddi_enable, 1);
	devm_gpiod_put(ctx->dev, ctx->vddi_enable);
	lcm_panel_vci_enable(ctx->dev);
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	panel->panel_initialized = true;

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	/* add for display fps cpunt */
	dsi_panel_fps_count(ctx, 0, 1);
	/* add for display state count*/
	if (!ctx->mi_count.panel_active_count_enable)
		dsi_panel_state_count(ctx, 1);

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

static const struct drm_display_mode default_mode = {
	.clock = 365384,
	.hdisplay = HACT,
	.hsync_start = HACT + MODE0_HFP,
	.hsync_end = HACT + MODE0_HFP + MODE0_HSA,
	.htotal = HACT + MODE0_HFP + MODE0_HSA + MODE0_HBP,
	.vdisplay = VACT,
	.vsync_start = VACT + MODE0_VFP,
	.vsync_end = VACT + MODE0_VFP + MODE0_VSA,
	.vtotal = VACT + MODE0_VFP + MODE0_VSA + MODE0_VBP,
	.vrefresh = MODE0_FPS,
};

static const struct drm_display_mode performance_mode = {
	.clock = 365384,
	.hdisplay = HACT,
	.hsync_start = HACT + MODE1_HFP,
	.hsync_end = HACT + MODE1_HFP + MODE1_HSA,
	.htotal = HACT + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT,
	.vsync_start = VACT + MODE1_VFP,
	.vsync_end = VACT + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT + MODE1_VFP + MODE1_VSA + MODE1_VBP,
	.vrefresh = MODE1_FPS,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.esd_check_enable = 0,
	.mi_esd_check_enable = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69500,
	.physical_height_um = 154600,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 40,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 989,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 651,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = DATA_RATE,
	.dyn_fps = {
		.switch_en = 1,
		.send_mode = 1,
		.send_cmd_need_delay = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 3, {0xF0, 0x5A, 0x5A} },
		.dfps_cmd_table[1] = {0, 2, {0x60, 0x21} },
		.dfps_cmd_table[2] = {0, 2, {0xF7, 0x0B} },
		.dfps_cmd_table[3] = {0, 3, {0xF0, 0xA5, 0xA5} },
	},
	.vdo_doze_enable = 1,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size=sizeof(top_rc_pattern),
	.corner_pattern_lt_addr=(void*)top_rc_pattern,
#endif
};

static struct mtk_panel_params ext_params_120hz = {
	.esd_check_enable = 0,
	.mi_esd_check_enable = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69500,
	.physical_height_um = 154600,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 34,
		.rct_on = 1,
		.bit_per_channel = 8,
		.dsc_line_buf_depth = 9,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 40,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 989,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 631,
		.slice_bpg_offset = 651,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 3,
		.flatness_maxqp = 12,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 11,
		.rc_quant_incr_limit1 = 11,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		},
	.data_rate = DATA_RATE,
	.dyn_fps = {
		.switch_en = 1,
		.send_mode = 1,
		.send_cmd_need_delay = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 3, {0xF0, 0x5A, 0x5A} },
		.dfps_cmd_table[1] = {0, 2, {0x60, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0xF7, 0x0B} },
		.dfps_cmd_table[3] = {0, 3, {0xF0, 0xA5, 0xA5} },
	},
	.vdo_doze_enable = 1,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size=sizeof(top_rc_pattern),
	.corner_pattern_lt_addr=(void*)top_rc_pattern,
#endif
};

static int panel_get_panel_info(struct drm_panel *panel, char *buf)
{
	int count = 0;
	struct lcm *ctx;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);

	return count;
}

static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level)
{
	struct lcm *ctx = panel_to_lcm(panel);
	char bl_tb[] = {0x51, 0x07, 0xff};
	int ret = 0;

	if (!panel->connector) {
		pr_err("%s, the connector is null\n", __func__);
		return -1;
	}

	dsi_panel_backlight_count(ctx, level);

	if (level >= 8) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
	}
	bl_tb[1] = (level >> 8) & 0xFF;
	bl_tb[2] = level & 0xFF;

	cmd_msg.channel = 0;
	cmd_msg.flags = 0;
	cmd_msg.tx_cmd_num = 1;

	cmd_msg.type[0] = 0x39;
	cmd_msg.tx_buf[0] = bl_tb;
	cmd_msg.tx_len[0] = 3;

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, false);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	pr_info("%s last_backligh = %d, level = %d, high_bl = 0x%x, low_bl = 0x%x \n", __func__, last_backlight, level, bl_tb[1], bl_tb[2]);
	last_backlight = level;

	return ret;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	bl_tb0[1] = level * 4 >> 8;
	bl_tb0[2] = level * 4 & 0xFF;

	if (!cb)
		return -1;

	pr_info("%s: level = %d\n", __func__, level);
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;

	if (mode == 0)
		ext->params = &ext_params;
	else if (mode == 1)
		ext->params = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_60_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);
	/* Frequency Select 120HZ*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
}

static void mode_switch_120_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);
	/* Frequency Select 60HZ*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0xA5, 0xA5);
}

static struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, dst_mode);

	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	if (m->vrefresh == 120) { /* 60 switch to 120 */
		mode_switch_60_to_120(panel, stage);
	} else if (m->vrefresh == 60) { /* 120 switch to 60 */
		mode_switch_120_to_60(panel, stage);
	} else
		ret = 1;

	return ret;
}

static int panel_hbm_fod_control(struct drm_panel *panel, bool en)
{
	int ret = 0;
	char hbm_on[] = {0x53, 0xE8};
	char hbm_off[] = {0x53, 0x28};
	char updtate_key[] = {0xF7, 0x0B};
	char bl_tb[] = {0x51, 0x07, 0xff};

	struct lcm *ctx = panel_to_lcm(panel);
	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return -1;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 3;

	if (en) {
		bl_tb[1] = 0x07;
		bl_tb[2] = 0xFF;

		cmd_msg->type[0] = 0x15;
		cmd_msg->tx_buf[0] = hbm_on;
		cmd_msg->tx_len[0] = ARRAYSIZE(hbm_on);

		cmd_msg->type[1] = 0x39;
		cmd_msg->tx_buf[1] = bl_tb;
		cmd_msg->tx_len[1] = ARRAYSIZE(bl_tb);

		cmd_msg->type[2] = 0x15;
		cmd_msg->tx_buf[2] = updtate_key;
		cmd_msg->tx_len[2] = ARRAYSIZE(updtate_key);
	} else {
		bl_tb[1] = bl_tb0[1];
		bl_tb[2] = bl_tb0[2];

		cmd_msg->type[0] = 0x15;
		cmd_msg->tx_buf[0] = hbm_off;
		cmd_msg->tx_len[0] = ARRAYSIZE(hbm_off);

		cmd_msg->type[1] = 0x15;
		cmd_msg->tx_buf[1] = updtate_key;
		cmd_msg->tx_len[1] = ARRAYSIZE(updtate_key);

		cmd_msg->type[2] = 0x39;
		cmd_msg->tx_buf[2] = bl_tb;
		cmd_msg->tx_len[2] = ARRAYSIZE(bl_tb);
	}

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}
	ctx->hbm_en = en;

done:
	vfree(cmd_msg);
	/* add for display hbm count */
	if (en)
		dsi_panel_HBM_count(ctx, 1, 0);
	else
		dsi_panel_HBM_count(ctx, 0, 0);

	pr_info("%s end -\n", __func__);
	return ret;
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

static void panel_set_crc_off(struct drm_panel *panel)
{
	int ret;
	char level_key_en[] = {0xF0, 0x5A, 0x5A};
	char level_key_dis[] = {0xF0, 0xA5, 0xA5};
	char crc_dis[] = {0x80, 0x00};
	char flat_mode_offset[] = {0xB0, 0x00, 0x88, 0xB1};
	char flat_mode_dis[] = {0xB1, 0x27, 0x0D};

	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 5;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = level_key_en;
	cmd_msg->tx_len[0] = ARRAYSIZE(level_key_en);

	cmd_msg->type[1] = 0x15;
	cmd_msg->tx_buf[1] = crc_dis;
	cmd_msg->tx_len[1] = ARRAYSIZE(crc_dis);

	cmd_msg->type[2] = 0x39;
	cmd_msg->tx_buf[2] = flat_mode_offset;
	cmd_msg->tx_len[2] = ARRAYSIZE(flat_mode_offset);

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = flat_mode_dis;
	cmd_msg->tx_len[3] = ARRAYSIZE(flat_mode_dis);

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = level_key_dis;
	cmd_msg->tx_len[4] = ARRAYSIZE(level_key_dis);

	ret = mtk_ddic_dsi_wait_te_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_crc_srgb(struct drm_panel *panel)
{
	int ret;
	char level_key_en[] = {0xF0, 0x5A, 0x5A};
	char level_key_dis[] = {0xF0, 0xA5, 0xA5};
	char crc_en[] = {0x80, 0x05};
	char crc_on[] = {0xB1, 0x00};
	char crc_offset[] = {0xB0, 0x00, 0x01, 0xB1};
	char crc_srgb[] = {0xB1, 0xBB, 0x08, 0x05, 0x34, 0xE2, 0x13, 0x07, 0x07, 0xAA,
			   0x3A, 0xE6, 0xCC, 0xC1, 0x0E, 0xB9, 0xF2, 0xE7, 0x17, 0xFF,
			   0xF6, 0xD3}; /* 0.3127 0.329 sRGB flat */
	char flat_mode_offset[] = {0xB0, 0x00, 0x88, 0xB1};
	char flat_mode_en[] = {0xB1, 0x27, 0x2D};

	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 8;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = level_key_en;
	cmd_msg->tx_len[0] = ARRAYSIZE(level_key_en);

	cmd_msg->type[1] = 0x15;
	cmd_msg->tx_buf[1] = crc_en;
	cmd_msg->tx_len[1] = ARRAYSIZE(crc_en);

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = crc_on;
	cmd_msg->tx_len[2] = ARRAYSIZE(crc_on);

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = crc_offset;
	cmd_msg->tx_len[3] = ARRAYSIZE(crc_offset);

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = crc_srgb;
	cmd_msg->tx_len[4] = ARRAYSIZE(crc_srgb);

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = flat_mode_offset;
	cmd_msg->tx_len[5] = ARRAYSIZE(flat_mode_offset);

	cmd_msg->type[6] = 0x39;
	cmd_msg->tx_buf[6] = flat_mode_en;
	cmd_msg->tx_len[6] = ARRAYSIZE(flat_mode_en);

	cmd_msg->type[7] = 0x39;
	cmd_msg->tx_buf[7] = level_key_dis;
	cmd_msg->tx_len[7] = ARRAYSIZE(level_key_dis);

	ret = mtk_ddic_dsi_wait_te_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_crc_p3(struct drm_panel *panel)
{
	int ret;
	char level_key_en[] = {0xF0, 0x5A, 0x5A};
	char level_key_dis[] = {0xF0, 0xA5, 0xA5};
	char crc_en[] = {0x80, 0x05};
	char crc_on[] = {0xB1, 0x00};
	char crc_offset[] = {0xB0, 0x00, 0x01, 0xB1};
	char crc_p3[] = {0xB1, 0xC9, 0x00, 0x00, 0x12, 0xCC, 0x02, 0x0A, 0x0B, 0xE9,
			 0x12, 0xE8, 0xE9, 0xE5, 0x02, 0xEE, 0xDE, 0xDC, 0x02, 0xFF,
			 0xFF, 0xFF}; /* 0.3 0.315 DCI-P3 */
	char flat_mode_offset[] = {0xB0, 0x00, 0x88, 0xB1};
	char flat_mode_dis[] = {0xB1, 0x27, 0x0D};

	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 8;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = level_key_en;
	cmd_msg->tx_len[0] = ARRAYSIZE(level_key_en);

	cmd_msg->type[1] = 0x15;
	cmd_msg->tx_buf[1] = crc_en;
	cmd_msg->tx_len[1] = ARRAYSIZE(crc_en);

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = crc_on;
	cmd_msg->tx_len[2] = ARRAYSIZE(crc_on);

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = crc_offset;
	cmd_msg->tx_len[3] = ARRAYSIZE(crc_offset);

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = crc_p3;
	cmd_msg->tx_len[4] = ARRAYSIZE(crc_p3);

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = flat_mode_offset;
	cmd_msg->tx_len[5] = ARRAYSIZE(flat_mode_offset);

	cmd_msg->type[6] = 0x39;
	cmd_msg->tx_buf[6] = flat_mode_dis;
	cmd_msg->tx_len[6] = ARRAYSIZE(flat_mode_dis);

	cmd_msg->type[7] = 0x39;
	cmd_msg->tx_buf[7] = level_key_dis;
	cmd_msg->tx_len[7] = ARRAYSIZE(level_key_dis);

	ret = mtk_ddic_dsi_wait_te_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_crc_p3_d65(struct drm_panel *panel)
{
	int ret;
	char level_key_en[] = {0xF0, 0x5A, 0x5A};
	char level_key_dis[] = {0xF0, 0xA5, 0xA5};
	char crc_en[] = {0x80, 0x05};
	char crc_on[] = {0xB1, 0x00};
	char crc_offset[] = {0xB0, 0x00, 0x01, 0xB1};
	char crc_p3_d65[] = {0xB1, 0xC5, 0x00, 0x00, 0x15, 0xC7, 0x02, 0x07, 0x0A, 0xB2,
			   0x20, 0xE9, 0xD9, 0xDD, 0x0D, 0xCC, 0xE8, 0xD9, 0x02, 0xFD,
			   0xF3, 0xDF}; /* 0.3127 0.329 DCI-P3 non-flat */
	char flat_mode_offset[] = {0xB0, 0x00, 0x88, 0xB1};
	char flat_mode_dis[] = {0xB1, 0x27, 0x0D};

	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 8;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = level_key_en;
	cmd_msg->tx_len[0] = ARRAYSIZE(level_key_en);

	cmd_msg->type[1] = 0x15;
	cmd_msg->tx_buf[1] = crc_en;
	cmd_msg->tx_len[1] = ARRAYSIZE(crc_en);

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = crc_on;
	cmd_msg->tx_len[2] = ARRAYSIZE(crc_on);

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = crc_offset;
	cmd_msg->tx_len[3] = ARRAYSIZE(crc_offset);

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = crc_p3_d65;
	cmd_msg->tx_len[4] = ARRAYSIZE(crc_p3_d65);

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = flat_mode_offset;
	cmd_msg->tx_len[5] = ARRAYSIZE(flat_mode_offset);

	cmd_msg->type[6] = 0x39;
	cmd_msg->tx_buf[6] = flat_mode_dis;
	cmd_msg->tx_len[6] = ARRAYSIZE(flat_mode_dis);

	cmd_msg->type[7] = 0x39;
	cmd_msg->tx_buf[7] = level_key_dis;
	cmd_msg->tx_len[7] = ARRAYSIZE(level_key_dis);

	ret = mtk_ddic_dsi_wait_te_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void panel_set_crc_p3_flat(struct drm_panel *panel)
{
	int ret;
	char level_key_en[] = {0xF0, 0x5A, 0x5A};
	char level_key_dis[] = {0xF0, 0xA5, 0xA5};
	char crc_en[] = {0x80, 0x05};
	char crc_on[] = {0xB1, 0x00};
	char crc_offset[] = {0xB0, 0x00, 0x01, 0xB1};
	char crc_p3_d65[] = {0xB1, 0xEC, 0x00, 0x00, 0x09, 0xE9, 0x03, 0x07, 0x07, 0xBE,
				 0x11, 0xEE, 0xD0, 0xEA, 0x05, 0xC8, 0xF3, 0xE6, 0x02, 0xFF,
				 0xF6, 0xD3}; /* 0.3127 0.329 DCI-P3 flat */
	char flat_mode_offset[] = {0xB0, 0x00, 0x88, 0xB1};
	char flat_mode_en[] = {0xB1, 0x27, 0x2D};

	struct mtk_ddic_dsi_msg *cmd_msg =
		vmalloc(sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s begin +\n", __func__);
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 8;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = level_key_en;
	cmd_msg->tx_len[0] = ARRAYSIZE(level_key_en);

	cmd_msg->type[1] = 0x15;
	cmd_msg->tx_buf[1] = crc_en;
	cmd_msg->tx_len[1] = ARRAYSIZE(crc_en);

	cmd_msg->type[2] = 0x15;
	cmd_msg->tx_buf[2] = crc_on;
	cmd_msg->tx_len[2] = ARRAYSIZE(crc_on);

	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = crc_offset;
	cmd_msg->tx_len[3] = ARRAYSIZE(crc_offset);

	cmd_msg->type[4] = 0x39;
	cmd_msg->tx_buf[4] = crc_p3_d65;
	cmd_msg->tx_len[4] = ARRAYSIZE(crc_p3_d65);

	cmd_msg->type[5] = 0x39;
	cmd_msg->tx_buf[5] = flat_mode_offset;
	cmd_msg->tx_len[5] = ARRAYSIZE(flat_mode_offset);

	cmd_msg->type[6] = 0x39;
	cmd_msg->tx_buf[6] = flat_mode_en;
	cmd_msg->tx_len[6] = ARRAYSIZE(flat_mode_en);

	cmd_msg->type[7] = 0x39;
	cmd_msg->tx_buf[7] = level_key_dis;
	cmd_msg->tx_len[7] = ARRAYSIZE(level_key_dis);

	ret = mtk_ddic_dsi_wait_te_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
}

static void lcm_esd_restore_backlight(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));

	pr_info("%s high_bl = 0x%x, low_bl = 0x%x \n", __func__, bl_tb0[1], bl_tb0[2]);

	return;
}

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
	struct lcm *ctx;
	int ret = 0;
	char bl_tb[] = {0x51, 0x00, 0x00};

	struct mtk_ddic_dsi_msg *cmd_msg = NULL;

	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}

	ctx = panel_to_lcm(panel);

	if (ctx->doze_brightness_state == doze_brightness) {
		pr_info("%s skip same doze_brightness set:%d\n", __func__, doze_brightness);
		return 0;
	}

	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return -1;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	pr_info("%s+ doze_brightness:%d\n", __func__, doze_brightness);

	if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
		bl_tb[1] = 0x01;
		bl_tb[2] = 0x10;
	} else if (DOZE_BRIGHTNESS_LBM == doze_brightness) {
		bl_tb[1] = 0x00;
		bl_tb[2] = 0x16;
	} else {
		if (last_backlight) {
			bl_tb[1] = bl_tb0[1];
			bl_tb[2] = bl_tb0[2];
		} else {
			ctx->doze_brightness_state = doze_brightness;
			goto done;
		}
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = bl_tb;
	cmd_msg->tx_len[0] = ARRAY_SIZE(bl_tb);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s error\n", __func__);
		goto  done;
	}
	ctx->doze_brightness_state = doze_brightness;

done:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);

	return ret;
}

static int panel_get_doze_brightness(struct drm_panel *panel, u32 *doze_brightness)
{
	struct lcm *ctx = panel_to_lcm(panel);
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	*doze_brightness = ctx->doze_brightness_state;
	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.setbacklight_control = lcm_setbacklight_control,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.reset = panel_ext_reset,
	.get_panel_info = panel_get_panel_info,
	.panel_set_crc_off = panel_set_crc_off,
	.panel_set_crc_srgb = panel_set_crc_srgb,
	.panel_set_crc_p3 = panel_set_crc_p3,
	.panel_set_crc_p3_d65 = panel_set_crc_p3_d65,
	.panel_set_crc_p3_flat = panel_set_crc_p3_flat,
	.hbm_fod_control = panel_hbm_fod_control,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.esd_restore_backlight = lcm_esd_restore_backlight,
};
#endif

struct panel_desc {
	const struct drm_display_mode *modes;
	unsigned int num_modes;

	unsigned int bpc;

	struct {
		unsigned int width;
		unsigned int height;
	} size;

	/**
	 * @prepare: the time (in milliseconds) that it takes for the panel to
	 *           become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

static int lcm_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;


	mode = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode);

	mode2 = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode2) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode.hdisplay,
			performance_mode.vdisplay,
			performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

	panel->connector->display_info.width_mm = 71;
	panel->connector->display_info.height_mm = 153;

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
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s %s+\n", __func__, panel_name);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			  | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->vddi_enable = devm_gpiod_get(dev, "vddi-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_enable)) {
		dev_err(dev, "cannot get vddi-enable-gpios %ld\n",
			PTR_ERR(ctx->vddi_enable));
		return PTR_ERR(ctx->vddi_enable);
	}
	devm_gpiod_put(dev, ctx->vddi_enable);

	ext_params.err_flag_irq_gpio = of_get_named_gpio_flags(
			dev->of_node, "mi,esd-err-irq-gpio",
			0, (enum of_gpio_flags *)&(ext_params.err_flag_irq_flags));

	ext_params_120hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;

	ext_params.err_flag_irq_gpio = of_get_named_gpio_flags(
			dev->of_node, "mi,esd-err-irq-gpio",
			0, (enum of_gpio_flags *)&(ext_params.err_flag_irq_flags));

	ext_params_120hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;

	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;
	ctx->panel_info = panel_name;
	ctx->doze_brightness_state = DOZE_TO_NORMAL;
	ctx->panel.panel_initialized = true;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	dsi_panel_count_init(ctx);
	pr_info("%s %s-\n", __func__, panel_name);

	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{
		.compatible = "k16_38_0c_0a_dsc_vdo,lcm",
	},
	{} };

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {

			.name = "k16_38_0c_0a_dsc_vdo,lcm",
			.owner = THIS_MODULE,
			.of_match_table = lcm_of_match,
		},
};

module_mipi_dsi_driver(lcm_driver);

