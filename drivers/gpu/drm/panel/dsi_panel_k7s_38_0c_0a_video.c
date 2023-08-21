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
#include <linux/of_graph.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#include <linux/notifier.h>
#include <linux/fb_notifier.h>
#include <linux/of_gpio.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_panel_ext.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_drm_graphics_base.h"
#endif

/* Porch Values Setting for Mode 0 Start */
#define DATA_RATE               1100
#define MODE0_FPS               60
#define MODE0_HACT              1080
#define MODE0_HFP               184
#define MODE0_HSA               80
#define MODE0_HBP               130
#define MODE0_VACT              2400
#define MODE0_VFP               1236
#define MODE0_VSA               2
#define MODE0_VBP               10
/* Porch Values Setting for Mode 0 End */

/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */

/* option function to read data from some panel address */
/* #define PANEL_SUPPORT_READBACK */

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vddi_enable;
	struct gpio_desc *vci_enable;
	bool prepared;
	bool enabled;

	int error;
	const char *panel_info;
};
static u32 fake_heigh = 2400;
static u32 fake_width = 1080;

//static bool need_fake_resolution;

static char bl_tb0[] = {0x51, 0x3, 0xff};
struct mtk_ddic_dsi_msg cmd_msg = {0};
const char *panel_name = "panel_name=dsi_panel_k7s_38_0c_0a_video";
extern bool esd_check_fail_open_backlight;

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
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
	//if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	//else
	//	ret = mipi_dsi_generic_write(dsi, data, len);
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

static void lcm_panel_init(struct lcm *ctx)
{
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(2);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(12);

	lcm_dcs_write_seq_static(ctx, 0x11);
	msleep(20);

	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x2C, 0xE0, 0x01);
  	lcm_dcs_write_seq_static(ctx, 0xE0, 0x00, 0x03, 0x95, 0x07, 0x85, 0x03, 0x39, 0x05, 0x6D, 0x05, 0x51);
  	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x41, 0xE0, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xE0, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5, 0xA5, 0xA5);


	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x01, 0x30);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5);

  	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x08, 0xEA, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0xF6, 0x89);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0A, 0xEA, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0x55, 0x01, 0x04);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0D, 0xEA, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0xFF, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x74, 0xE1, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0x3F);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x10, 0xEA, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0B, 0xCE, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xCE, 0x11, 0xFC, 0xFC);  
	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5, 0xA5, 0xA5);
	//lcm_dcs_write_seq_static(ctx, 0x9F, 0x5A, 0x5A);
	//lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	//lcm_dcs_write_seq_static(ctx, 0x9F, 0xA5, 0xA5);

	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x07);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5);

//	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A, 0x5A, 0x5A);
//	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x9D, 0xE0, 0x01);
//	lcm_dcs_write_seq_static(ctx, 0xE0, 0x00);
//	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5, 0xA5, 0xA5);	

  	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A, 0x5A, 0x5A);///
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x32, 0xF6, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x24, 0x00, 0x19, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5, 0xA5, 0xA5);
  
	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x09, 0xCD);
	lcm_dcs_write_seq_static(ctx, 0xCD, 0x08, 0x48, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5);
  
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x01);  

	lcm_dcs_write_seq_static(ctx, 0x9E, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
										0x04, 0x38, 0x00, 0x28, 0x02, 0x1C, 0x02, 0x1C,
										0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0xDD,
										0x00, 0x07, 0x00, 0x0C, 0x02, 0x77, 0x02, 0x8B,
										0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
										0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
										0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
										0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
										0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
										0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
										0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
										0x00);
  
	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0D, 0xB2, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x20); // 32 Frame Dimming
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00, 0x0C, 0xB2, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x30); // ELVSS Dimming On
	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5);	
	lcm_dcs_write_seq_static(ctx, 0x53, 0x30);
	//lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0xFF);
	//printk("esd_check_fail_open_backlight is %d\n",esd_check_fail_open_backlight);//这边打印看看是否变化
	if (esd_check_fail_open_backlight) {
	    lcm_dcs_write_seq_static(ctx, 0x51, 0x03, 0xFF);
		esd_check_fail_open_backlight = false;
	}
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x07);

	msleep(140);


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

	ctx->enabled = false;

	return 0;
}

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (!ctx->prepared)
		return 0;
	struct fb_drm_notify_data g_notify_data;
	int event = FB_DRM_BLANK_POWERDOWN;
	g_notify_data.data = &event;
	//START tp fb suspend
	//pr_info("-----FTS----primary_display_suspend_early");
	fb_drm_notifier_call_chain(FB_DRM_EVENT_BLANK, &g_notify_data);

	pr_info("%s+\n", __func__);
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(10);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(10);
	gpiod_set_value(ctx->vci_enable, 0);
	//gpiod_set_value(ctx->vddi_enable, 0);
	ctx->error = 0;
	ctx->prepared = false;
  	//pr_info("-----FTS----primary_display_suspend");
	fb_drm_notifier_call_chain(FB_DRM_EARLY_EVENT_BLANK, &g_notify_data);
	pr_info("%s-\n", __func__);

	return 0;

}

static int panel_k7s_dsi_poweron(struct drm_panel *panel)
{
		struct lcm *ctx = panel_to_lcm(panel);
		gpiod_set_value(ctx->vddi_enable, 1);
		gpiod_set_value(ctx->vci_enable, 1);

		return 0;

}

static int lcm_prepare(struct drm_panel *panel)
{
		struct lcm *ctx = panel_to_lcm(panel);
		int ret;

		pr_info("%s+\n", __func__);
		if (ctx->prepared)
			return 0;
		struct fb_drm_notify_data g_notify_data;
		int event = FB_DRM_BLANK_UNBLANK;
		g_notify_data.data = &event;
		//START tp fb suspend
		//pr_info("-----FTS----primary_display_resume_early");
		fb_drm_notifier_call_chain(FB_DRM_EARLY_EVENT_BLANK, &g_notify_data);
		//printk("---FTS---mt6781");

//		gpiod_set_value(ctx->vddi_enable, 1);
//		gpiod_set_value(ctx->vci_enable, 1);
		//lcm_panel_vci_enable(ctx->dev);
		lcm_panel_init(ctx);

		ret = ctx->error;
		if (ret < 0)
			lcm_unprepare(panel);

		ctx->prepared = true;
 		//pr_info("-----FTS----primary_display_resume");
		fb_drm_notifier_call_chain(FB_DRM_EVENT_BLANK, &g_notify_data);

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

static struct drm_display_mode default_mode = {
	.clock = 322629,
	.hdisplay = MODE0_HACT,
	.hsync_start = MODE0_HACT + MODE0_HFP,
	.hsync_end = MODE0_HACT + MODE0_HFP + MODE0_HSA,
	.htotal = MODE0_HACT + MODE0_HFP + MODE0_HSA + MODE0_HBP,
	.vdisplay = MODE0_VACT,
	.vsync_start = MODE0_VACT + MODE0_VFP,
	.vsync_end = MODE0_VACT + MODE0_VFP + MODE0_VSA,
	.vtotal = MODE0_VACT + MODE0_VFP + MODE0_VSA + MODE0_VBP,
	.vrefresh = MODE0_FPS,
};

static struct drm_display_mode performance_mode = {
	.clock = 322629,
	.hdisplay = 1080,
	.hsync_start = MODE0_HACT + MODE0_HFP,
	.hsync_end = MODE0_HACT + MODE0_HFP + MODE0_HSA,
	.htotal = MODE0_HACT + MODE0_HFP + MODE0_HSA + MODE0_HBP,
	.vdisplay = 2400,
	.vsync_start = 2400 + 20,
	.vsync_end = 2400 + 20 + 2,
	.vtotal = 2400 + 20 + 2 + 10,
	.vrefresh = 90,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {

			.cmd = 0x0A, .count = 1, .para_list[0] = 0x9D,
		},
        .mi_esd_check_enable = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 67000,
	.physical_height_um = 149000,
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
				.switch_en=1,
				.vact_timing_fps=90,
				.send_mode = 1,
				.send_cmd_need_delay = 1,
				.dfps_cmd_table[0] = {0, 5 , {0xF1, 0x5A, 0x5A, 0x5A, 0x5A}},
				.dfps_cmd_table[1] = {0, 2 , {0x60, 0x21}},
				.dfps_cmd_table[2] = {0, 2 , {0xF7, 0x07}},
				.dfps_cmd_table[3] = {0, 5 , {0xF1, 0xA5, 0xA5, 0xA5, 0xA5}},
				},


};

static struct mtk_panel_params ext_params_90hz = {
	.cust_esd_check = 1,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {

			.cmd = 0x0A, .count = 1, .para_list[0] = 0x9D,
		},
        .mi_esd_check_enable = 1,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 67000,
	.physical_height_um = 149000,
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
				.switch_en=1,
				.vact_timing_fps=90,
				.send_mode = 1,
				.send_cmd_need_delay = 1,
				.dfps_cmd_table[0] = {0, 5 , {0xF1, 0x5A, 0x5A, 0x5A, 0x5A}},
				.dfps_cmd_table[1] = {0, 2 , {0x60, 0x01}},
				.dfps_cmd_table[2] = {0, 2 , {0xF7, 0x07}},	
				.dfps_cmd_table[3] = {0, 5 , {0xF1, 0xA5, 0xA5, 0xA5, 0xA5}},
				},

};

extern int mtk_ddic_dsi_send_cmd(struct mtk_ddic_dsi_msg *cmd_msg,
			bool blocking);
static int panel_hbm_control(struct drm_panel *panel, bool en)
{
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	int ret;
	char hbm_tb[] = {0x53, 0xF0};
	char bl_tb[] = {0x51, 0x07, 0xff};

	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return 0;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	if (en) {
		hbm_tb[1] = 0xF0;
	}
	else {
		bl_tb[1] = bl_tb0[1];
		bl_tb[2] = bl_tb0[2];
		hbm_tb[1] = 0x30;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 2;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = hbm_tb;
	cmd_msg->tx_len[0] = 2;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = bl_tb;
	cmd_msg->tx_len[1] = 3;

	pr_info("%s+,hbm_tb = 0x%x, high_bl = 0x%x, low_bl = 0x%x \n", __func__, hbm_tb[1], bl_tb[1], bl_tb[2]);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	vfree(cmd_msg);
	pr_info("%s-\n", __func__);

	return 0;
}

static int panel_aod_control(bool en)
{
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	int ret;
	char bl_tb[] = {0x51, 0x00, 0xff};
	pr_info("%s+\n", __func__);

	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return 0;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));

	if (en) {
		//bl_tb[] = {0x51, 0x00, 0xff};
		bl_tb[2] = 0xff;
	}
	else {
		//bl_tb[] = {0x51, 0x00, 0x2f};
		bl_tb[2] = 0x2f;
	}

	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = bl_tb;
	cmd_msg->tx_len[0] = 3;

	pr_info("%s+, reg = 0x%x, high_bl = 0x%x, low_bl = 0x%x\n", __func__, bl_tb[0], bl_tb[1], bl_tb[2]);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	vfree(cmd_msg);
	pr_info("%s-\n", __func__);

	return 0;
}

static int panel_srgb_control(struct drm_panel *panel)
{
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	int ret;
	char leavel2_key_on[] = {0xF1, 0x5A,0x5A};
	char flat_gamma1[] = {0xB0, 0x00, 0x88, 0xB1, 0x01};
	char flat_gamma2[] = {0xB1, 0x27, 0x2D};
	char leavel2_key_off[] = {0xF1, 0xA5,0xA5};
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return 0;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));


	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 4;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = leavel2_key_on;
	cmd_msg->tx_len[0] = 3;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = flat_gamma1;
	cmd_msg->tx_len[1] = 5;

	cmd_msg->type[2] = 0x39;
	cmd_msg->tx_buf[2] = flat_gamma2;
	cmd_msg->tx_len[2] = 3;
	
	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = leavel2_key_off;
	cmd_msg->tx_len[3] = 3;
	//pr_info("%s+,hbm_tb = 0x%x, high_bl = 0x%x, low_bl = 0x%x \n", __func__, hbm_tb[1], bl_tb[1], bl_tb[2]);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	vfree(cmd_msg);
	pr_info("%s-\n", __func__);

	return 0;
}

static int panel_p3_control(struct drm_panel *panel)
{

	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	int ret;
	char leavel2_key_on[] = {0xF1, 0x5A,0x5A};
	char flat_gamma1[] = {0xB0, 0x00, 0x88, 0xB1, 0x01};
	char flat_gamma2[] = {0xB1, 0x27, 0x0D};
	char leavel2_key_off[] = {0xF1, 0xA5,0xA5};
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return 0;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));


	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 4;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = leavel2_key_on;
	cmd_msg->tx_len[0] = 3;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = flat_gamma1;
	cmd_msg->tx_len[1] = 5;

	cmd_msg->type[2] = 0x39;
	cmd_msg->tx_buf[2] = flat_gamma2;
	cmd_msg->tx_len[2] = 3;
	
	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = leavel2_key_off;
	cmd_msg->tx_len[3] = 3;
	//pr_info("%s+,hbm_tb = 0x%x, high_bl = 0x%x, low_bl = 0x%x \n", __func__, hbm_tb[1], bl_tb[1], bl_tb[2]);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	vfree(cmd_msg);
	pr_info("%s-\n", __func__);

	return 0;
}

static int panel_crc_off_control(struct drm_panel *panel)
{
	struct mtk_ddic_dsi_msg *cmd_msg =
			vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	int ret;
	char leavel2_key_on[] = {0xF1, 0x5A,0x5A};
	char flat_gamma1[] = {0xB0, 0x00, 0x88, 0xB1, 0x01};
	char flat_gamma2[] = {0xB1, 0x27, 0x0D};
	char leavel2_key_off[] = {0xF1, 0xA5,0xA5};
	if (unlikely(!cmd_msg)) {
		pr_err("%s vmalloc mtk_ddic_dsi_msg is failed, return\n", __func__);
		return 0;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));


	cmd_msg->channel = 0;
	cmd_msg->flags = 0;
	cmd_msg->tx_cmd_num = 4;

	cmd_msg->type[0] = 0x39;
	cmd_msg->tx_buf[0] = leavel2_key_on;
	cmd_msg->tx_len[0] = 3;

	cmd_msg->type[1] = 0x39;
	cmd_msg->tx_buf[1] = flat_gamma1;
	cmd_msg->tx_len[1] = 5;

	cmd_msg->type[2] = 0x39;
	cmd_msg->tx_buf[2] = flat_gamma2;
	cmd_msg->tx_len[2] = 3;
	
	cmd_msg->type[3] = 0x39;
	cmd_msg->tx_buf[3] = leavel2_key_off;
	cmd_msg->tx_len[3] = 3;
	//pr_info("%s+,hbm_tb = 0x%x, high_bl = 0x%x, low_bl = 0x%x \n", __func__, hbm_tb[1], bl_tb[1], bl_tb[2]);

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	vfree(cmd_msg);
	pr_info("%s-\n", __func__);

	return 0;
}


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
/*
static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level)
{
	char bl_tb[] = {0x51, 0x07, 0xff};
	int ret = 0;

	if (!panel->connector) {
		pr_err("%s, the connector is null\n", __func__);
		return -1;
	}

	if (level > 8) {
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

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true);
	if (ret != 0) {
		pr_err("%s mtk_ddic_dsi_send_cmd error\n", __func__);
	}

	pr_info("%s level = %d, high_bl = 0x%x, low_bl = 0x%x \n", __func__, level, bl_tb[1], bl_tb[2]);

	return ret;
}
*/
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
#if defined(CONFIG_BACKLIGHT_SUPPORT_2047_FEATURE)
	bl_tb0[1] = (level >> 8) & 0x7;
	bl_tb0[2] = level & 0xFF;
#else
	bl_tb0[1] = level * 7 >> 8;
	bl_tb0[2] = level * 7 & 0xFF;
#endif

	if (!cb)
		return -1;

	pr_info("%s: K7S for backlight level = %d,1=%x,2=%x,3=%x\n", __func__, level,bl_tb0[0],bl_tb0[1],bl_tb0[2]);
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static struct drm_display_mode *get_mode_by_id(struct drm_panel *panel,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;
	pr_info("%s: K7S for get_mode_by_id mode==%d\n", __func__,mode);

	list_for_each_entry(m, &panel->connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}
static int mtk_panel_ext_param_set(struct drm_panel *panel, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	//struct drm_display_mode *m = get_mode_by_id(panel, mode);
	pr_info("%s: K7S for mtk_panel_ext_param_set\n", __func__);

	if (mode == 0)
		ext->params = &ext_params;
	else if (mode == 1)
		ext->params = &ext_params_90hz;
	else
		ret = 1;

	return ret;
}

static void mode_switch_60_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s: K7S for mode_switch_60_to_90\n", __func__);

	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A, 0x5A, 0x5A);
 	lcm_dcs_write_seq_static(ctx, 0x60, 0x01);
 	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5, 0xA5, 0xA5);

}

static void mode_switch_90_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s: K7S for mode_switch_90_to_60\n", __func__);

	lcm_dcs_write_seq_static(ctx, 0xF1, 0x5A, 0x5A, 0x5A, 0x5A);
 	lcm_dcs_write_seq_static(ctx, 0x60, 0x21);
 	lcm_dcs_write_seq_static(ctx, 0xF1, 0xA5, 0xA5, 0xA5, 0xA5);
}

static int mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	pr_info("%s: K7S for mode_switch\n", __func__);
	struct drm_display_mode *m = get_mode_by_id(panel, dst_mode);

	if (m->vrefresh == 90) { /* 60 switch to 120 */
		mode_switch_60_to_90(panel, stage);
	} else if (m->vrefresh == 60) { /* 120 switch to 60 */
		mode_switch_90_to_60(panel, stage);
	} else
		ret = 1;

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

static void lcm_esd_restore_backlight(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	//lcm_dcs_write(ctx, bl_level, ARRAY_SIZE(bl_level));

	//pr_info("%s high_bl = 0x%x, low_bl = 0x%x \n", __func__, bl_level[1], bl_level[2]);
        lcm_dcs_write_seq_static(ctx, 0x51, 0x03, 0xFF);
 	 pr_info(" zgq add:lcm_esd_restore_backlight \n");

	return;
}

static struct mtk_panel_funcs ext_funcs = {
	//.setbacklight_control = lcm_setbacklight_control,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.reset = panel_ext_reset,
	.esd_restore_backlight = lcm_esd_restore_backlight,
	.get_panel_info = panel_get_panel_info,
	.hbm_control = panel_hbm_control,
  	.aod_control = panel_aod_control,
	.panel_set_crc_srgb = panel_srgb_control,
	.panel_set_crc_p3 = panel_p3_control,
	.panel_set_crc_off = panel_crc_off_control,
	.k7s_dsi_poweron = panel_k7s_dsi_poweron,
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

static void change_drm_disp_mode_params(struct drm_display_mode *mode)
{
	pr_info("%s: K7S for change_drm_disp_mode_params\n", __func__);

	if (fake_heigh > 0 && fake_heigh < MODE0_VACT) {
		mode->vsync_start = mode->vsync_start - mode->vdisplay
					+ fake_heigh;
		mode->vsync_end = mode->vsync_end - mode->vdisplay + fake_heigh;
		mode->vtotal = mode->vtotal - mode->vdisplay + fake_heigh;
		mode->vdisplay = fake_heigh;
	}
	if (fake_width > 0 && fake_width < MODE0_HACT) {
		mode->hsync_start = mode->hsync_start - mode->hdisplay
					+ fake_width;
		mode->hsync_end = mode->hsync_end - mode->hdisplay + fake_width;
		mode->htotal = mode->htotal - mode->hdisplay + fake_width;
		mode->hdisplay = fake_width;
	}
}


static int lcm_get_modes(struct drm_panel *panel)
{
	pr_info("%s: K7S for lcm_get_modes\n", __func__);

	struct drm_display_mode *mode;
	struct drm_display_mode *mode2;
	pr_info("%s+ K7S for\n", __func__);

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

static char g_lcd_id[128];
static struct kobject *msm_lcd_name = NULL;
static ssize_t lcd_name_show(struct kobject *dev,
		struct kobj_attribute *attr, char *buf)
{
   ssize_t ret = 0;
   sprintf(buf, "%s\n", g_lcd_id);
   ret = strlen(buf) + 1;
   return ret;
}
static struct kobj_attribute dev_attr_lcd_name=
      __ATTR(lcd_name, S_IRUGO, lcd_name_show, NULL);

static int msm_lcd_name_create_sysfs(void){
   int ret;
   msm_lcd_name=kobject_create_and_add("android_lcd",NULL);

   if(msm_lcd_name==NULL){
     pr_info("msm_lcd_name_create_sysfs_ failed\n");
     ret=-ENOMEM;
     return ret;
   }
   ret=sysfs_create_file(msm_lcd_name,&dev_attr_lcd_name.attr);
   if(ret){
    pr_info("%s failed \n",__func__);
    kobject_del(msm_lcd_name);
   }
   return 0;
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
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


	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST
			  | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_CLOCK_NON_CONTINUOUS;


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

	ctx->vddi_enable = devm_gpiod_get(dev, "vddi", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddi_enable)) {
		dev_err(dev, "cannot get vddi-enable-gpios %ld\n",
			PTR_ERR(ctx->vddi_enable));
		return PTR_ERR(ctx->vddi_enable);
	}

	ctx->vci_enable = devm_gpiod_get(dev, "vci", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_enable)) {
		dev_err(dev, "cannot get vci-enable-gpios %ld\n",
			PTR_ERR(ctx->vci_enable));
		return PTR_ERR(ctx->vci_enable);
	}
    
	ext_params.err_flag_irq_gpio = of_get_named_gpio_flags(
			dev->of_node, "mi,esd-err-irq-gpio",
			0, (enum of_gpio_flags *)&(ext_params.err_flag_irq_flags));

	ext_params_90hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_90hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	
	ctx->prepared = true;
	ctx->enabled = true;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;
	ctx->panel_info = panel_name;

	strcpy(g_lcd_id,panel_name);
	msm_lcd_name_create_sysfs();

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

	pr_info("%s: panel_k7_38_0c_0a_fhdp_video-\n", __func__);

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
		.compatible = "dsi_panel_k7s_38_0c_0a_video",
	},
	{} };

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "dsi_panel_k7s_38_0c_0a_video",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);
