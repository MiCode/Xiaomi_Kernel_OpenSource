// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024 Xiaomi Inc.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drm_panel.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_modes.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
#include <linux/hqsysfs.h>

#include <uapi/drm/mi_disp.h>

#include "../mediatek/mediatek_v2/mi_notifier_odm.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#define REGFLAG_DELAY               0xFFFC
#define REGFLAG_UDELAY              0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD
#define REGFLAG_RESET_LOW           0xFFFE
#define REGFLAG_RESET_HIGH          0xFFFF

#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2460

#define PHYSICAL_WIDTH              69336
#define PHYSICAL_HEIGHT             157932

#define DATA_RATE                   764
#define HSA                         16
#define HBP                         16
#define HFP                         166
#define VSA                         10
#define VBP                         10

/*Parameter setting for mode 0 Start*/
#define MODE_0_FPS                  60
#define MODE_0_VFP                  1321
/*Parameter setting for mode 0 End*/

/*Parameter setting for mode 1 Start*/
#define MODE_1_FPS                  90
#define MODE_1_VFP                  54
/*Parameter setting for mode 1 End*/

/*Parameter setting for mode 2 Start*/
#define MODE_2_FPS                  48
#define MODE_2_VFP                  2271
/*Parameter setting for mode 2 End*/

/*Parameter setting for mode 3 Start*/
#define MODE_3_FPS                  36
#define MODE_3_VFP                  3855
/*Parameter setting for mode 3 End*/

#define CABC_CTRL_REG               0x55

#define HBM_MAP_MAX_BRIGHTNESS      4095
#define NORMAL_MAX_BRIGHTNESS       2047
#define REG_MAX_BRIGHTNESS          2047
#define REG_HBM_BRIGHTNESS          1735
#define REG_NORMAL_BRIGHTNESS       1420

static u32 panel_id = 0xFF;

/*N19A code for HQ- by p-xielihui at 2024/5/7 start*/
/*4096 is a value that will not be used*/
static unsigned int last_bl = 4096;
/*N19A code for HQ- by p-xielihui at 2024/5/7 end*/
static bool need_enable_dimming_flag = false;

#define OPEN_DIMMING                0x2C
#define CLOSE_DIMMING               0x24

#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 34
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         8
#define DSC_DSC_LINE_BUF_DEPTH      9
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
//define DSC_PIC_HEIGHT
//define DSC_PIC_WIDTH
#define DSC_SLICE_HEIGHT            20
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              540
#define DSC_XMIT_DELAY              170
#define DSC_DEC_DELAY               526
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      113
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          1294
#define DSC_SLICE_BPG_OFFSET        1302
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            7072
#define DSC_FLATNESS_MINQP          3
#define DSC_FLATNESS_MAXQP          12
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    11
#define DSC_RC_QUANT_INCR_LIMIT1    11
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *avee_gpio;
	struct gpio_desc *avdd_gpio;
	bool prepared;
	bool enabled;

	struct mutex panel_lock;
	const char *panel_info;

	int cabc_status;

	int error;
};

static int current_fps = 60;
static const char *panel_name = "panel_name=dsi_n19a_43_02_0c_dsc_vdo";
static struct drm_panel *this_panel = NULL;
static atomic_t gesture_status = ATOMIC_INIT(0);
static atomic_t proximity_status = ATOMIC_INIT(0);
struct drm_notifier_data g_notify_data1;
static int blank;

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
		if (ARRAY_SIZE(d) > 2)                                    \
			udelay(100);                                      \
	})

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

static void lcm_dcs_write(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	int ret = 0;
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
	mdelay(12);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(5);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(3);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(3);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(12);

	mutex_lock(&ctx->panel_lock);

	// optimization
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xC1, 0x89, 0x28, 0x00, 0x14, 0x00, 0xAA, 0x02, 0x0E, 0x00, 0x71, 0x00, 0x07, 0x05, 0x0E, 0x05, 0x16);
	lcm_dcs_write_seq_static(ctx, 0xC2, 0x1B, 0xA0);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x23);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x60);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x84);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x09, 0xBE);
	lcm_dcs_write_seq_static(ctx, 0x0A, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0x0B, 0xB2);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x7B);
	lcm_dcs_write_seq_static(ctx, 0x0D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x12, 0x9F);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x65);
	lcm_dcs_write_seq_static(ctx, 0x16, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0x19, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x1A, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x1B, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x1E, 0x16);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x18);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x1A);
	lcm_dcs_write_seq_static(ctx, 0x21, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x22, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x23, 0x25);
	lcm_dcs_write_seq_static(ctx, 0x24, 0x29);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x2E);
	lcm_dcs_write_seq_static(ctx, 0x26, 0x32);
	lcm_dcs_write_seq_static(ctx, 0x27, 0x37);
	lcm_dcs_write_seq_static(ctx, 0x28, 0x3C);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x0D);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0x30, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x31, 0xFE);
	lcm_dcs_write_seq_static(ctx, 0x32, 0xFD);
	lcm_dcs_write_seq_static(ctx, 0x33, 0xFC);
	lcm_dcs_write_seq_static(ctx, 0x34, 0xFB);
	lcm_dcs_write_seq_static(ctx, 0x35, 0xFA);
	lcm_dcs_write_seq_static(ctx, 0x36, 0xF9);
	lcm_dcs_write_seq_static(ctx, 0x36, 0xF9);
	lcm_dcs_write_seq_static(ctx, 0x37, 0xF8);
	lcm_dcs_write_seq_static(ctx, 0x38, 0xF7);
	lcm_dcs_write_seq_static(ctx, 0x39, 0xF6);
	lcm_dcs_write_seq_static(ctx, 0x3A, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0xF2);
	lcm_dcs_write_seq_static(ctx, 0x3D, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x3F, 0xEE);
	lcm_dcs_write_seq_static(ctx, 0x40, 0xEA);
	lcm_dcs_write_seq_static(ctx, 0x41, 0xE6);
	lcm_dcs_write_seq_static(ctx, 0x45, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x46, 0xFA);
	lcm_dcs_write_seq_static(ctx, 0x47, 0xF5);
	lcm_dcs_write_seq_static(ctx, 0x48, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x49, 0xEB);
	lcm_dcs_write_seq_static(ctx, 0x4A, 0xE6);
	lcm_dcs_write_seq_static(ctx, 0x4B, 0xE1);
	lcm_dcs_write_seq_static(ctx, 0x4C, 0xDC);
	lcm_dcs_write_seq_static(ctx, 0x4D, 0xD7);
	lcm_dcs_write_seq_static(ctx, 0x4E, 0xD2);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0xCD);
	lcm_dcs_write_seq_static(ctx, 0x50, 0xC8);
	lcm_dcs_write_seq_static(ctx, 0x51, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0x52, 0xBE);
	lcm_dcs_write_seq_static(ctx, 0x53, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0x54, 0x66);
	lcm_dcs_write_seq_static(ctx, 0x58, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0x59, 0xFA);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0xF5);
	lcm_dcs_write_seq_static(ctx, 0x5B, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0x5C, 0xEB);
	lcm_dcs_write_seq_static(ctx, 0x5D, 0xE6);
	lcm_dcs_write_seq_static(ctx, 0x5E, 0xE1);
	lcm_dcs_write_seq_static(ctx, 0x5F, 0xDC);
	lcm_dcs_write_seq_static(ctx, 0x60, 0xD7);
	lcm_dcs_write_seq_static(ctx, 0x61, 0xD2);
	lcm_dcs_write_seq_static(ctx, 0x62, 0xCD);
	lcm_dcs_write_seq_static(ctx, 0x63, 0xC8);
	lcm_dcs_write_seq_static(ctx, 0x64, 0xC3);
	lcm_dcs_write_seq_static(ctx, 0x65, 0xBE);
	lcm_dcs_write_seq_static(ctx, 0x66, 0xB9);
	lcm_dcs_write_seq_static(ctx, 0x67, 0xB4);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0xF0);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0x00);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);

	lcm_dcs_write_seq_static(ctx, 0X11);
	mdelay(105);
	lcm_dcs_write_seq_static(ctx, 0x29);
	mdelay(40);
	mutex_unlock(&ctx->panel_lock);

}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[kernel/lcm]%s enter\n", __func__);

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}

void panel_gesture_mode_set_by_nt36672s(bool gst_mode)
{
	atomic_set(&gesture_status, gst_mode);
}
EXPORT_SYMBOL_GPL(panel_gesture_mode_set_by_nt36672s);

static bool panel_check_gesture_mode(void)
{
	bool ret = false;

	pr_info("[kernel/lcm]%s enter\n", __func__);
	ret = atomic_read(&gesture_status);

	return ret;
}

void panel_proximity_mode_set_by_nt36672s(bool prx_mode)
{
	atomic_set(&proximity_status, prx_mode);
}
EXPORT_SYMBOL_GPL(panel_proximity_mode_set_by_nt36672s);

static bool panel_check_proximity_mode(void)
{
	bool ret = false;

	pr_info("[kernel/lcm]%s enter\n", __func__);
	ret = atomic_read(&proximity_status);

	return ret;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[kernel/lcm]%s enter\n", __func__);

	if (!ctx->prepared)
		return 0;

	blank = DRM_BLANK_POWERDOWN;
	g_notify_data1.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
	pr_info("[XMFP] : %s ++++ blank = DRM_BLANK_POWERDOWN ++++", __func__);

	if (panel_check_proximity_mode()) {
		pr_info("[kernel/lcm]%s proximity mode on, proximity_status = %d\n",
					__func__, proximity_status);
		mutex_lock(&ctx->panel_lock);
		lcm_dcs_write_seq_static(ctx, 0x28);
		msleep(120);
		mutex_unlock(&ctx->panel_lock);
	} else if (panel_check_gesture_mode()) {
		pr_info("[kernel/lcm]%s gesture mode on, gesture_status = %d\n",
					__func__, gesture_status);
		mutex_lock(&ctx->panel_lock);
		lcm_dcs_write_seq_static(ctx, 0x28);
		msleep(1);
		lcm_dcs_write_seq_static(ctx, 0x10);
		msleep(120);
		mutex_unlock(&ctx->panel_lock);
	} else {
		mutex_lock(&ctx->panel_lock);
		lcm_dcs_write_seq_static(ctx, 0x28);
		msleep(1);
		lcm_dcs_write_seq_static(ctx, 0x10);
		msleep(120);
		mutex_unlock(&ctx->panel_lock);
		gpiod_set_value(ctx->avee_gpio, 0);
		mdelay(1);
		gpiod_set_value(ctx->avdd_gpio, 0);
	}

	ctx->error = 0;
	ctx->prepared = false;
	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret = 0;

	pr_info("[kernel/lcm]%s enter\n", __func__);

	if (ctx->prepared)
		return 0;

	gpiod_set_value(ctx->avdd_gpio, 1);
	mdelay(1);

	gpiod_set_value(ctx->avee_gpio, 1);

	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	blank = DRM_BLANK_UNBLANK;
	g_notify_data1.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
	pr_info("[XMFP] : %s ++++ blank = DRM_BLANK_UNBLANK ++++", __func__);

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[kernel/lcm]%s enter\n", __func__);

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
	.clock = (FRAME_WIDTH + HFP + HSA + HBP) *
		(FRAME_HEIGHT + MODE_0_VFP + VSA + VBP) * MODE_0_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
};

static const struct drm_display_mode performance_mode_1 = {
	.clock = (FRAME_WIDTH + HFP + HSA + HBP) *
		(FRAME_HEIGHT + MODE_1_VFP + VSA + VBP) * MODE_1_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
};

static const struct drm_display_mode performance_mode_2 = {
	.clock = (FRAME_WIDTH + HFP + HSA + HBP) *
		(FRAME_HEIGHT + MODE_2_VFP + VSA + VBP) * MODE_2_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
};

static const struct drm_display_mode performance_mode_3 = {
	.clock = (FRAME_WIDTH + HFP + HSA + HBP) *
		(FRAME_HEIGHT + MODE_3_VFP + VSA + VBP) * MODE_3_FPS / 1000,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,
	.hsync_end = FRAME_WIDTH + HFP + HSA,
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_3_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_3_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_3_VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params = {
	.vfp_low_power = MODE_3_VFP,//36hz
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = DSC_ENABLE,
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
	.data_rate = DATA_RATE,
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = MODE_1_FPS,
	},
	/* dsi_hbp is hbp_wc, cal hbp according to hbp_wc, ref:4997538 */
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE,
		.vfp = MODE_0_VFP,
	},
	.phy_timcon = {
		.clk_hs_post = 0x24,
	},
};

static struct mtk_panel_params ext_params_mode_1 = {

	.vfp_low_power = MODE_3_VFP,//36hz
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = DSC_ENABLE,
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
	.data_rate = DATA_RATE,
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = MODE_1_FPS,
	},
	/* dsi_hbp is hbp_wc, cal hbp according to hbp_wc, ref:4997538 */
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE,
		.vfp = MODE_1_VFP,
	},
	.phy_timcon = {
		.clk_hs_post = 0x24,
	},
};

static struct mtk_panel_params ext_params_mode_2 = {

	.vfp_low_power = MODE_3_VFP,//36hz
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = DSC_ENABLE,
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
	.data_rate = DATA_RATE,
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = MODE_1_FPS,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE,
		.vfp = MODE_2_VFP,
	},
	.phy_timcon = {
		.clk_hs_post = 0x24,
	},
};

static struct mtk_panel_params ext_params_mode_3 = {
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A,
		.count = 1,
		.para_list[0] = 0x9C,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = DSC_ENABLE,
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
	.data_rate = DATA_RATE,
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = MODE_1_FPS,
	},
	.dyn = {
		.switch_en = 1,
		.data_rate = DATA_RATE,
		.vfp = MODE_3_VFP,
	},
	.phy_timcon = {
		.clk_hs_post = 0x24,
	},
};

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{

	struct lcm *ctx = panel_to_lcm(this_panel);

	char bl_tb0[] = {0x51, 0x07, 0xFF};
	char bl_en0[] = {0x53, 0x24};
	/*N19A code for HQ-378474 by p-xielihui at 2024/04/15 start*/
	unsigned int level_in = level;
	/*N19A code for HQ-378474 by p-xielihui at 2023/04/15 end*/

	if (!cb){
		pr_err("[kernel/lcm] cb error");
		return -1;
	}

	if (level > HBM_MAP_MAX_BRIGHTNESS) {
		pr_err("[kernel/lcm]%s err level;", __func__);
		return -EPERM;
	} else if (level > NORMAL_MAX_BRIGHTNESS) {
		pr_info("[kernel/lcm]%s hbm_on, level = %d", __func__, level);
		level = (level - REG_MAX_BRIGHTNESS) * (REG_HBM_BRIGHTNESS - REG_NORMAL_BRIGHTNESS)
			 / (HBM_MAP_MAX_BRIGHTNESS - REG_MAX_BRIGHTNESS) + REG_NORMAL_BRIGHTNESS;
	} else {
		pr_info("[kernel/lcm]%s hbm_off, level = %d", __func__, level);
		level = level * REG_NORMAL_BRIGHTNESS / REG_MAX_BRIGHTNESS;
	}

	bl_tb0[1] = level >> 8 & 0x07;
	bl_tb0[2] = level & 0xFF;

	pr_info("[kernel/lcm]%s backlight = %d, bl_tb0[1] = 0x%x, bl_tb0[2] = 0x%x\n",
				__func__, level, bl_tb0[1], bl_tb0[2]);

	mutex_lock(&ctx->panel_lock);

	/*N19A code for HQ-378474 by p-xielihui at 2024/04/15 start*/
	/*To prevent jitter, setting the backlight will add delay and segmentation. 
	The 53 register controls the output and delay function of the PWM. 
	When the screen is turn off, level_in is set to 0, and the value of 0 is assigned to last_bl; 
	When the screen is turn on, level_in is the target value and last_bl is 0; 
	When triggering ESD, it is in an abnormal state and the backlight will not be set to 0. 
	The target value set is the previous value, which can meet the requirement of level_in==last_bl;*/
	if (level_in == 0 || last_bl == 0 || last_bl == level_in) {
		bl_en0[1] = CLOSE_DIMMING;
		need_enable_dimming_flag = true;
		cb(dsi, handle, bl_en0, ARRAY_SIZE(bl_en0));
	} else if (level_in > 0 && need_enable_dimming_flag) {
		bl_en0[1] = OPEN_DIMMING;
		need_enable_dimming_flag = false;
		cb(dsi, handle, bl_en0, ARRAY_SIZE(bl_en0));
	}

	last_bl = level_in;
	/*N19A code for HQ-378474 by p-xielihui at 2023/04/15 end*/

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	mutex_unlock(&ctx->panel_lock);

	return 0;
}

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	pr_info("[kernel/lcm]%s enter\n", __func__);

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
		pr_info("[kernel/lcm]%s mode is %d\n", __func__, m);
			return m;
		i++;
	}
	pr_info("[kernel/lcm]%s get mode fail\n", __func__);
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			 struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (m == NULL) {
		pr_info("[kernel/lcm]%s:%d invalid display_mode\n", __func__, __LINE__);
		return -1;
	}
	if (drm_mode_vrefresh(m) == MODE_0_FPS)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == MODE_1_FPS)
		ext->params = &ext_params_mode_1;
	else if (drm_mode_vrefresh(m) == MODE_2_FPS)
		ext->params = &ext_params_mode_2;
	else if (drm_mode_vrefresh(m) == MODE_3_FPS)
		ext->params = &ext_params_mode_3;
	else
		ret = 1;
	if (!ret)
		current_fps = drm_mode_vrefresh(m);

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("[kernel/lcm]%s enter\n", __func__);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_get_panel_info(struct drm_panel *panel, char *buf)
{
	int count = 0;
	struct lcm *ctx;

	if (!panel) {
		pr_err(": panel is NULL\n", __func__);
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);

	return count;
}

static int panel_set_cabc_mode(void *dsi, dcs_write_gce cb,
	void *handle, u32 mode)
{
	struct lcm *ctx = panel_to_lcm(this_panel);
	char cabc_state[] = {CABC_CTRL_REG, mode};

	pr_info("[kernel/lcm]%s enter, set cabc_mode: %d\n", __func__, mode);

	if (!ctx) {
		pr_err("invalid params\n");
		return -ENODEV;
	}

	if (!cb){
		pr_err("[kernel/lcm] cb error");
		return -ENODEV;
	}

	switch (mode)
	{
		case DDIC_CABC_UI_ON:
		case DDIC_CABC_STILL_ON:
		case DDIC_CABC_MOVIE_ON:
			cabc_state[1] = mode & 0xFF;
			ctx->cabc_status = mode;
			break;
		default:
			cabc_state[1] = 0x00;
			ctx->cabc_status = DDIC_CABC_OFF;
			break;
	}

	mutex_lock(&ctx->panel_lock);
	cb(dsi, handle, cabc_state, ARRAY_SIZE(cabc_state));
	mutex_unlock(&ctx->panel_lock);

	pr_info("[kernel/lcm]%s exit, final cabc_status is: %d\n", __func__, ctx->cabc_status);

	return 0;
}

static int panel_get_cabc_mode(struct drm_panel *panel, int *mode)
{
	struct lcm *ctx;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	*mode = ctx->cabc_status;

	pr_info("[kernel/lcm]%s enter, cabc_mode: %d\n", __func__, ctx->cabc_status);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.reset = panel_ext_reset,
	.get_panel_info = panel_get_panel_info,
	.set_cabc_mode = panel_set_cabc_mode,
	.get_cabc_mode = panel_get_cabc_mode,
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

static int lcm_get_modes(struct drm_panel *panel,
			struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;
	struct drm_display_mode *mode_3;
	static int mode_count = 0;

	pr_info("[kernel/lcm]%s enter\n", __func__);

	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);
	mode_count++;

	mode_1 = drm_mode_duplicate(connector->dev, &performance_mode_1);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_1.hdisplay,
			 performance_mode_1.vdisplay,
			 drm_mode_vrefresh(&performance_mode_1));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);
	mode_count++;

	mode_2 = drm_mode_duplicate(connector->dev, &performance_mode_2);
	if (!mode_2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_2.hdisplay,
			 performance_mode_2.vdisplay,
			 drm_mode_vrefresh(&performance_mode_2));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_2);
	mode_count++;

	mode_3 = drm_mode_duplicate(connector->dev, &performance_mode_3);
	if (!mode_3) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode_3.hdisplay,
			 performance_mode_3.vdisplay,
			 drm_mode_vrefresh(&performance_mode_3));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_3);
	mode_3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_3);
	mode_count++;

	connector->display_info.width_mm = (unsigned int)PHYSICAL_WIDTH/1000;;
	connector->display_info.height_mm = (unsigned int)PHYSICAL_HEIGHT/100029;

	return mode_count;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

extern int panel_event;
static ssize_t panel_event_show(struct device *device,
			struct device_attribute *attr,
			char *buf)
{
	ssize_t ret = 0;
	struct drm_connector *connector = dev_get_drvdata(device);
	if (!connector) {
		pr_info("%s-%d connector is NULL \r\n",__func__, __LINE__);
		return ret;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", panel_event);
}

static DEVICE_ATTR_RO(panel_event);

static struct attribute *panel_attrs[] = {
	&dev_attr_panel_event.attr,
        NULL,
};

static const struct attribute_group panel1_attr_group = {
        .attrs = panel_attrs,
};

extern unsigned int mi_get_panel_id(void);
static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	int ret = 0;

	panel_id = mi_get_panel_id();
	if (panel_id == NT36672S_TRULY_PANEL)
		pr_info("[%s]: It is panel_n19a_43_02_0c, panel_id = 0x%x\n", __func__, panel_id);
	else {
		pr_info("[%s]: It is not panel_n19a_43_02_0c, panel_id = 0x%x\n", __func__, panel_id);
		return -ENODEV;
	}

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_ASIS);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->avee_gpio = devm_gpiod_get(dev, "avee", GPIOD_ASIS);
	if (IS_ERR(ctx->avee_gpio)) {
		dev_info(dev, "cannot get avee_gpios %ld\n",
			PTR_ERR(ctx->avee_gpio));
		return PTR_ERR(ctx->avee_gpio);
	}
	devm_gpiod_put(dev, ctx->avee_gpio);

	ctx->avdd_gpio = devm_gpiod_get(dev, "avdd", GPIOD_ASIS);
	if (IS_ERR(ctx->avdd_gpio)) {
		dev_info(dev, "cannot get avdd_gpios %ld\n",
			PTR_ERR(ctx->avdd_gpio));
		return PTR_ERR(ctx->avdd_gpio);
	}
	devm_gpiod_put(dev, ctx->avdd_gpio);

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
	mutex_init(&ctx->panel_lock);
	ctx->panel_info = panel_name;
	this_panel = &ctx->panel;
	hq_regiser_hw_info(HWID_LCM, "incell, vendor:43, IC:02");

	ctx->cabc_status = DDIC_CABC_OFF;

	ret = sysfs_create_group(&dev->kobj, &panel1_attr_group);
	if (ret)
		return ret;

	pr_info("[kernel/lcm] %s exit\n", __func__);

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
	{.compatible = "n19a-43-02-0c-dsc-vdo,lcm",},
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-n19a-43-02-0c-dsc-vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_DESCRIPTION("truly nt36672s vdo 90hz 6382 Panel Driver");
MODULE_LICENSE("GPL v2");
