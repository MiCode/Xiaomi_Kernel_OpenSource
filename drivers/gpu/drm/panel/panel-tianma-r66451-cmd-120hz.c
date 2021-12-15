/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"

#include "include/panel-tianma-r66451-cmd-120hz.h"
#endif

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */

struct tianma {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;

	int error;
};

static char bl_tb0[] = {0x51, 0xf, 0xff};

#define tianma_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

#define tianma_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		tianma_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

static inline struct tianma *panel_to_tianma(struct drm_panel *panel)
{
	return container_of(panel, struct tianma, panel);
}

#ifdef PANEL_SUPPORT_READBACK
static int tianma_dcs_read(struct tianma *ctx, u8 cmd, void *data, size_t len)
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

static void tianma_panel_get_data(struct tianma *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = tianma_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif

static void tianma_dcs_write(struct tianma *ctx, const void *data, size_t len)
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

static void tianma_panel_init(struct tianma *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	msleep(200);

	/* set PLL to 190M */
	tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xB6, 0x51, 0x00, 0x06, 0x23, 0x8A,
		0x13, 0x1A, 0x05, 0x04, 0xFA, 0x05, 0x20);

	tianma_dcs_write_seq(ctx, bl_tb0[0], bl_tb0[1], bl_tb0[2]);
	tianma_dcs_write_seq_static(ctx, 0x53, 0x04);
	tianma_dcs_write_seq_static(ctx, 0x35, 0x00);
	tianma_dcs_write_seq_static(ctx, 0x44, 0x08, 0x66);
	tianma_dcs_write_seq_static(ctx, 0x2a, 0x00, 0x00, 0x04, 0x37);
	tianma_dcs_write_seq_static(ctx, 0x2b, 0x00, 0x00, 0x09, 0x23);

	msleep(200);

	tianma_dcs_write_seq_static(ctx, 0xB0, 0x80);
	tianma_dcs_write_seq_static(ctx, 0xD4, 0x93);
	tianma_dcs_write_seq_static(ctx, 0x50, 0x42, 0x58, 0x81, 0x2D, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x6B, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0xFF, 0xD4,
			0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x53,
			0x18, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

	/* set PPS to table and choose table 0 */
	tianma_dcs_write_seq_static(ctx, 0xF7, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xF8, 0x11, 0x00, 0x00, 0x89, 0x30,
		0x80, 0x09, 0x24, 0x04, 0x38, 0x00, 0x14, 0x02, 0x1c, 0x02,
		0x1c, 0x02, 0x00, 0x02, 0x0e, 0x00, 0x20, 0x01, 0xe8, 0x00,
		0x07, 0x00, 0x0c, 0x05, 0x0e, 0x05, 0x16, 0x18, 0x00, 0x10,
		0xf0, 0x03, 0x0c, 0x20, 0x00, 0x06, 0x0b, 0x0b, 0x33, 0x0e,
		0x1c, 0x2a, 0x38, 0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79,
		0x7b, 0x7d, 0x7e, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40, 0x09,
		0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8, 0x1a, 0x38, 0x1a,
		0x78, 0x1a, 0xb6, 0x2a, 0xf6, 0x2b, 0x34, 0x2b, 0x74, 0x3b,
		0x74, 0x6b, 0xf4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00);

	/* Turn on DSC */
	tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xEB, 0x8B, 0x8B);

	/* Flash QE setting */
	tianma_dcs_write_seq_static(ctx, 0xDF, 0x50, 0x40);
	tianma_dcs_write_seq_static(ctx, 0xF3, 0x50, 0x00, 0x00, 0x00, 0x00);
	tianma_dcs_write_seq_static(ctx, 0xF2, 0x11);

	usleep_range(10 * 1000, 15 * 1000);

	tianma_dcs_write_seq_static(ctx, 0xF3, 0x01, 0x00, 0x00, 0x00, 0x01);
	tianma_dcs_write_seq_static(ctx, 0xF4, 0x00, 0x02);
	tianma_dcs_write_seq_static(ctx, 0xF2, 0x19);

	usleep_range(20 * 1000, 25 * 1000);

	tianma_dcs_write_seq_static(ctx, 0xDF, 0x50, 0x42);
	tianma_dcs_write_seq_static(ctx, 0xB0, 0x04);
	tianma_dcs_write_seq_static(ctx, 0xE4, 0x00, 0x0B);
	tianma_dcs_write_seq_static(ctx, 0xB0, 0x84);
	tianma_dcs_write_seq_static(ctx, 0xE6, 0x01);
	tianma_dcs_write_seq_static(ctx, 0x11);

	msleep(120);

	tianma_dcs_write_seq_static(ctx, 0x29);
}

static int tianma_disable(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

static int tianma_unprepare(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (!ctx->prepared)
		return 0;

	tianma_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	tianma_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(200);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->error = 0;
	ctx->prepared = false;

	return 0;
}

static int tianma_prepare(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	tianma_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		tianma_unprepare(panel);

	ctx->prepared = true;

#ifdef PANEL_SUPPORT_READBACK
	tianma_panel_get_data(ctx);
#endif

	return ret;
}

static int tianma_enable(struct drm_panel *panel)
{
	struct tianma *ctx = panel_to_tianma(panel);

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
	.clock = 163530,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
	.vrefresh = MODE_0_FPS,
};

static const struct drm_display_mode switch_mode_1 = {
	.clock = 245295,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
	.vrefresh = MODE_1_FPS,
};

static const struct drm_display_mode switch_mode_2 = {
	.clock = 327060,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
	.vrefresh = MODE_2_FPS,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

			.cmd = 0x53, .count = 1, .para_list[0] = 0x24,
		},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.dyn_fps = {
		.data_rate = MODE_0_DATA_RATE,
	},
	.data_rate = MODE_2_DATA_RATE,
	.dyn = {
		.switch_en = 1,
		.data_rate = MODE_0_DATA_RATE + 10,
	},

};
static struct mtk_panel_params ext_params_mode_1 = {
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

			.cmd = 0x53, .count = 1, .para_list[0] = 0x24,
		},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.dyn_fps = {
		.data_rate = MODE_1_DATA_RATE,
	},
	.data_rate = MODE_2_DATA_RATE,
	.dyn = {
		.switch_en = 1,
		.data_rate = MODE_1_DATA_RATE + 10,
	},
};

static struct mtk_panel_params ext_params_mode_2 = {
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {

			.cmd = 0x53, .count = 1, .para_list[0] = 0x24,
		},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable                =  DSC_ENABLE,
		.ver                   =  DSC_VER,
		.slice_mode            =  DSC_SLICE_MODE,
		.rgb_swap              =  DSC_RGB_SWAP,
		.dsc_cfg               =  DSC_DSC_CFG,
		.rct_on                =  DSC_RCT_ON,
		.bit_per_channel       =  DSC_BIT_PER_CHANNEL,
		.dsc_line_buf_depth    =  DSC_DSC_LINE_BUF_DEPTH,
		.bp_enable             =  DSC_BP_ENABLE,
		.bit_per_pixel         =  DSC_BIT_PER_PIXEL,
		.pic_height            =  FRAME_HEIGHT,
		.pic_width             =  FRAME_WIDTH,
		.slice_height          =  DSC_SLICE_HEIGHT,
		.slice_width           =  DSC_SLICE_WIDTH,
		.chunk_size            =  DSC_CHUNK_SIZE,
		.xmit_delay            =  DSC_XMIT_DELAY,
		.dec_delay             =  DSC_DEC_DELAY,
		.scale_value           =  DSC_SCALE_VALUE,
		.increment_interval    =  DSC_INCREMENT_INTERVAL,
		.decrement_interval    =  DSC_DECREMENT_INTERVAL,
		.line_bpg_offset       =  DSC_LINE_BPG_OFFSET,
		.nfl_bpg_offset        =  DSC_NFL_BPG_OFFSET,
		.slice_bpg_offset      =  DSC_SLICE_BPG_OFFSET,
		.initial_offset        =  DSC_INITIAL_OFFSET,
		.final_offset          =  DSC_FINAL_OFFSET,
		.flatness_minqp        =  DSC_FLATNESS_MINQP,
		.flatness_maxqp        =  DSC_FLATNESS_MAXQP,
		.rc_model_size         =  DSC_RC_MODEL_SIZE,
		.rc_edge_factor        =  DSC_RC_EDGE_FACTOR,
		.rc_quant_incr_limit0  =  DSC_RC_QUANT_INCR_LIMIT0,
		.rc_quant_incr_limit1  =  DSC_RC_QUANT_INCR_LIMIT1,
		.rc_tgt_offset_hi      =  DSC_RC_TGT_OFFSET_HI,
		.rc_tgt_offset_lo      =  DSC_RC_TGT_OFFSET_LO,
	},
	.dyn_fps = {
		.data_rate = MODE_2_DATA_RATE,
	},
	.data_rate = MODE_2_DATA_RATE,
	.dyn = {
		.switch_en = 1,
		.data_rate = MODE_2_DATA_RATE + 10,
	},
};

static int tianma_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	if (level > 255)
		level = 255;

	level = level * 4095 / 255;
	bl_tb0[1] = ((level >> 8) & 0xf);
	bl_tb0[2] = (level & 0xff);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}
struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	pr_info("%s, %d, failed to get mode:%d, total:%u\n", __func__, __LINE__, mode, i);
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, mode);

	if (m->vrefresh == MODE_0_FPS)
		ext->params = &ext_params;
	else if (m->vrefresh == MODE_1_FPS)
		ext->params = &ext_params_mode_1;
	else if (m->vrefresh == MODE_2_FPS)
		ext->params = &ext_params_mode_2;
	else
		ret = 1;

	return ret;
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	if (stage == BEFORE_DSI_POWERDOWN) {
		struct tianma *ctx = panel_to_tianma(panel);

		/* set PLL to 380M */
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xB6, 0x6A, 0x00, 0x06, 0x23,
			0x8A, 0x13, 0x1A, 0x05, 0x04, 0xFA, 0x05, 0x20);

		/* switch to 120hz */
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x04);
		tianma_dcs_write_seq_static(ctx, 0xC1, 0x94, 0x42, 0x00,
			0x16, 0x05, 0x00, 0x00, 0x00, 0x10, 0x00, 0x10,
			0x00, 0xAA, 0x8A, 0x02, 0x10, 0x00, 0x10, 0x00,
			0x00, 0x3F, 0x3F, 0x03, 0xFF, 0x03, 0xFF, 0x23,
			0xFF, 0x03, 0xFF, 0x23, 0xFF, 0x03, 0xFF, 0x00,
			0x40, 0x40, 0x00, 0x00, 0x10, 0x01, 0x00, 0x0C);
		tianma_dcs_write_seq_static(ctx, 0xC2, 0x09, 0x24, 0x0E,
			0x00, 0x00, 0x0E);
		tianma_dcs_write_seq_static(ctx, 0xC4, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x02, 0x00, 0x00, 0x00, 0x35, 0x00, 0x01);
		tianma_dcs_write_seq_static(ctx, 0xCF, 0x64, 0x0B, 0x00,
			0x28, 0x02, 0x4B, 0x02, 0xDE, 0x0B, 0x77, 0x0B,
			0x8B, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
			0x04, 0x04, 0x05, 0x05, 0x05, 0x00, 0x3D, 0x00,
			0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x01,
			0x00, 0x01, 0x00, 0x03, 0x98, 0x03, 0xA9, 0x03,
			0xA9, 0x03, 0xA9, 0x03, 0xA9, 0x00, 0x3D, 0x00,
			0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x01,
			0x00, 0x01, 0x00, 0x03, 0x98, 0x03, 0xA9, 0x03,
			0xA9, 0x03, 0xA9, 0x03, 0xA9, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x1C, 0x1C, 0x1C,
			0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C,
			0x1C, 0x00, 0x88, 0x00, 0xB1, 0x00, 0xB1, 0x09,
			0xA6, 0x09, 0xA6, 0x09, 0xA4, 0x09, 0xA4, 0x09,
			0xA4, 0x09, 0xA4, 0x09, 0xA4, 0x09, 0xA4, 0x0F,
			0xC3, 0x19);
		tianma_dcs_write_seq_static(ctx, 0xD7, 0x00, 0x69, 0x34,
			0x00, 0xA0, 0x0A, 0x00, 0x00, 0x39);
		tianma_dcs_write_seq_static(ctx, 0xD8, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3A, 0x00,
			0x3A, 0x00, 0x3A, 0x00, 0x3A, 0x00, 0x3A, 0x05,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x20, 0x40, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x0F, 0x00, 0x32, 0x00, 0x00, 0x00, 0x17,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xBB, 0x59, 0xC8, 0xC8,
			0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0x4A,
			0x48, 0x46, 0x44, 0x42, 0x40, 0x3E, 0x3C, 0x3A,
			0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0x04, 0x00, 0x02, 0x02, 0x00, 0x04,
			0x69, 0x5A, 0x00, 0x0B, 0x76, 0x0F, 0xFF, 0x0F,
			0xFF, 0x0F, 0xFF, 0x14, 0x81, 0xF4);
		tianma_dcs_write_seq_static(ctx, 0xE8, 0x00, 0x02);
		tianma_dcs_write_seq_static(ctx, 0xE4, 0x00, 0x0B);
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x84);
		tianma_dcs_write_seq_static(ctx, 0xE4, 0x33, 0xB4, 0x00,
			0x00, 0x00, 0x39, 0x04, 0x09, 0x34);
		tianma_dcs_write_seq_static(ctx, 0xE6, 0x01);


		// tianma_dcs_write_seq_static(ctx, 0xD4, 0x93, 0x93, 0x60,
			// 0x1E, 0xE1, 0x02, 0x08, 0x00, 0x00, 0x02, 0x22,
			// 0x02, 0xEC, 0x03, 0x83, 0x04, 0x00, 0x04, 0x00,
			// 0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x01, 0x00,
			// 0x01, 0x00);
	}
}
static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		/* set PLL to 285M */
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xB6, 0x5A, 0x00, 0x06,
		0x23, 0x8A, 0x13, 0x1A, 0x05, 0x04, 0xFA, 0x05, 0x20);

		/* switch to 90hz */
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x04);
		tianma_dcs_write_seq_static(ctx, 0xF1, 0x2A);
		tianma_dcs_write_seq_static(ctx, 0xC1, 0x0C);
		tianma_dcs_write_seq_static(ctx, 0xC2, 0x09, 0x24, 0x0E,
			0x00, 0x00, 0x0E);
		tianma_dcs_write_seq_static(ctx, 0xC1, 0x94, 0x42, 0x00,
			0x16, 0x05);
		tianma_dcs_write_seq_static(ctx, 0xCF, 0x64, 0x0B, 0x00,
			0x28, 0x02, 0x4B, 0x02, 0xDE, 0x0B, 0x77, 0x0B,
			0x8B, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
			0x02, 0x02, 0x03, 0x03, 0x03, 0x00, 0x3D, 0x00,
			0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x01,
			0x00, 0x01, 0x00, 0x03, 0x98, 0x03, 0x98, 0x03,
			0x98, 0x03, 0x98, 0x03, 0x98, 0x00, 0x3D, 0x00,
			0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x01,
			0x00, 0x01, 0x00, 0x03, 0x98, 0x03, 0x98, 0x03,
			0x98, 0x03, 0x98, 0x03, 0x98, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x1C, 0x1C, 0x1C,
			0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C,
			0x1C, 0x00, 0x88, 0x00, 0xB1, 0x00, 0xB1, 0x09,
			0xA6, 0x09, 0xA6, 0x09, 0xA4, 0x09, 0xA4, 0x09,
			0xA4, 0x09, 0xA4, 0x09, 0xA4, 0x09, 0xA4, 0x0F,
			0xC3, 0x19);
		tianma_dcs_write_seq_static(ctx, 0xC4, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x02, 0x00, 0x00, 0x00, 0x48, 0x00, 0x01);
		tianma_dcs_write_seq_static(ctx, 0xD7, 0x00, 0x69, 0x34,
			0x00, 0xA0, 0x0A, 0x00, 0x00, 0x4E);
		tianma_dcs_write_seq_static(ctx, 0xD8, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x00,
			0x4D, 0x00, 0x4D, 0x00, 0x4D, 0x00, 0x4D, 0x05,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x4E, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x16, 0x00, 0x44, 0x00, 0x00, 0x00, 0x1F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xBB, 0x59, 0xC8, 0xC8,
			0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0x4A,
			0x48, 0x46, 0x44, 0x42, 0x40, 0x3E, 0x3C, 0x3A,
			0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0x04, 0x00, 0x02, 0x02, 0x00, 0x04,
			0x69, 0x5A, 0x00, 0x0B, 0x76, 0x0F, 0xFF, 0x0F,
			0xFF, 0x0F, 0xFF, 0x14, 0x81, 0xF4);
		tianma_dcs_write_seq_static(ctx, 0xE8, 0x00, 0x02);
		tianma_dcs_write_seq_static(ctx, 0xE4, 0x00, 0x0B);
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x84);
		tianma_dcs_write_seq_static(ctx, 0xE4, 0x33, 0xB4, 0x00,
			0x00, 0x00, 0x4E, 0x04, 0x04, 0x9A);
		tianma_dcs_write_seq_static(ctx, 0xE6, 0x01);
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct tianma *ctx = panel_to_tianma(panel);

	if (stage == BEFORE_DSI_POWERDOWN) {
		/* set PLL to 190M */
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xB6, 0x52, 0x00, 0x06, 0x23,
			0x8A, 0x13, 0x1A, 0x05, 0x04, 0xFA, 0x05, 0x20);

		/* switch to 60hz */
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x04);
		tianma_dcs_write_seq_static(ctx, 0xF1, 0x2A);
		tianma_dcs_write_seq_static(ctx, 0xC1, 0x0C);
		tianma_dcs_write_seq_static(ctx, 0xC2, 0x09, 0x24, 0x0E,
			0x00, 0x00, 0x0E);
		tianma_dcs_write_seq_static(ctx, 0xC1, 0x94, 0x42, 0x00,
			0x16, 0x05);
		tianma_dcs_write_seq_static(ctx, 0xCF, 0x64, 0x0B, 0x00,
			0x28, 0x02, 0x4B, 0x02, 0xDE, 0x0B, 0x77, 0x0B,
			0x8B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x3D, 0x00,
			0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x01,
			0x00, 0x01, 0x00, 0x03, 0x98, 0x03, 0x98, 0x03,
			0x98, 0x03, 0x98, 0x03, 0x98, 0x00, 0x3D, 0x00,
			0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x00, 0xCD, 0x01,
			0x00, 0x01, 0x00, 0x03, 0x98, 0x03, 0x98, 0x03,
			0x98, 0x03, 0x98, 0x03, 0x98, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x01, 0x42, 0x01,
			0x42, 0x01, 0x42, 0x01, 0x42, 0x1C, 0x1C, 0x1C,
			0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C,
			0x1C, 0x00, 0x88, 0x00, 0xB1, 0x00, 0xB1, 0x09,
			0xA6, 0x09, 0xA6, 0x09, 0xA4, 0x09, 0xA4, 0x09,
			0xA4, 0x09, 0xA4, 0x09, 0xA4, 0x09, 0xA4, 0x0F,
			0xC3, 0x19);
		tianma_dcs_write_seq_static(ctx, 0xC4, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x02, 0x00, 0x00, 0x00, 0x68, 0x00, 0x01);
		tianma_dcs_write_seq_static(ctx, 0xD7, 0x00, 0x69, 0x34,
			0x00, 0xA0, 0x0A, 0x00, 0x00, 0x75);
		tianma_dcs_write_seq_static(ctx, 0xD8, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x74, 0x00,
			0x74, 0x00, 0x74, 0x00, 0x74, 0x00, 0x74, 0x05,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x75, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x22, 0x00, 0x65, 0x00, 0x00, 0x00, 0x2F,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xBB, 0x59, 0xC8, 0xC8,
			0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0xC8, 0x4A,
			0x48, 0x46, 0x44, 0x42, 0x40, 0x3E, 0x3C, 0x3A,
			0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
			0xFF, 0xFF, 0x04, 0x00, 0x01, 0x01, 0x00, 0x04,
			0x69, 0x5A, 0x00, 0x0B, 0x76, 0x0F, 0xFF, 0x0F,
			0xFF, 0x0F, 0xFF, 0x14, 0x81, 0xF4);
		tianma_dcs_write_seq_static(ctx, 0xE8, 0x00, 0x02);
		tianma_dcs_write_seq_static(ctx, 0xE4, 0x00, 0x0B);
		tianma_dcs_write_seq_static(ctx, 0xB0, 0x84);
		tianma_dcs_write_seq_static(ctx, 0xE4, 0x33, 0xB4, 0x00,
			0x00, 0x00, 0x75, 0x04, 0x00, 0x00);
		tianma_dcs_write_seq_static(ctx, 0xE6, 0x01);
	}
}

static int mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(panel, dst_mode);
	if (cur_mode == dst_mode)
		return ret;

	if (m == NULL)
		return -EINVAL;

	if (m->vrefresh == MODE_0_FPS) { /*switch to 60 */
		mode_switch_to_60(panel, stage);
	} else if (m->vrefresh == MODE_1_FPS) { /*switch to 90 */
		mode_switch_to_90(panel, stage);
	} else if (m->vrefresh == MODE_2_FPS) { /*switch to 120 */
		mode_switch_to_120(panel, stage);
	} else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct tianma *ctx = panel_to_tianma(panel);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = tianma_setbacklight_cmdq,
	/* Not real backlight cmd in AOD, just for QC purpose */
	.set_aod_light_mode = tianma_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.reset = panel_ext_reset,
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

static int tianma_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;
	struct drm_display_mode *mode_3;

	mode_1 = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode_1) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode_1);

	mode_2 = drm_mode_duplicate(panel->drm, &switch_mode_1);
	if (!mode_2) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_1.hdisplay,
			switch_mode_1.vdisplay,
			switch_mode_1.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode_2);

	mode_3 = drm_mode_duplicate(panel->drm, &switch_mode_2);
	if (!mode_3) {
		dev_err(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			switch_mode_2.hdisplay,
			switch_mode_2.vdisplay,
			switch_mode_2.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode_3);
	mode_3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode_3);
	panel->connector->display_info.width_mm = 70;
	panel->connector->display_info.height_mm = 152;

	return 3;
}

static const struct drm_panel_funcs tianma_drm_funcs = {
	.disable = tianma_disable,
	.unprepare = tianma_unprepare,
	.prepare = tianma_prepare,
	.enable = tianma_enable,
	.get_modes = tianma_get_modes,
};

static int tianma_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct tianma *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct tianma), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

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
	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &tianma_drm_funcs;

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

	pr_info("%s-\n", __func__);

	return ret;
}

static int tianma_remove(struct mipi_dsi_device *dsi)
{
	struct tianma *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id tianma_of_match[] = {
	{
		.compatible = "tianma,r66451,cmd,120hz",
	},
	{} };

MODULE_DEVICE_TABLE(of, tianma_of_match);

static struct mipi_dsi_driver tianma_driver = {
	.probe = tianma_probe,
	.remove = tianma_remove,
	.driver = {

			.name = "panel-tianma-r66451-cmd-120hz",
			.owner = THIS_MODULE,
			.of_match_table = tianma_of_match,
		},
};

module_mipi_dsi_driver(tianma_driver);

MODULE_AUTHOR("Elon Hsu <elon.hsu@mediatek.com>");
MODULE_DESCRIPTION("tianma r66451 CMD 120HZ AMOLED Panel Driver");
MODULE_LICENSE("GPL v2");
