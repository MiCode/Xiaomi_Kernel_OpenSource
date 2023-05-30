
// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */
#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <linux/double_click.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_crtc.h"
#ifdef CONFIG_MI_DISP
#include <uapi/drm/mi_disp.h>
#endif
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
extern int get_panel_dead_flag(void);
static const char *panel_name = "panel_name=dsi_l16_42_02_0a_dsc_vdo";
static char led_wp_info_str[64] = {0};
extern int ktz8863a_bl_bias_conf(void);
extern int ktz8863a_bias_enable(int enable);
extern int ktz8863a_brightness_set(int level);
extern int ktz8863a_reg_write_bytes(unsigned char addr, unsigned char value);
extern int ktz8863a_reg_read_bytes(unsigned char addr, char *value);
#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2460
#define PHYSICAL_WIDTH              67392
#define PHYSICAL_HEIGHT             153504
#define DATA_RATE                   1100
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
#define DSC_SLICE_HEIGHT            20
#define DSC_SLICE_WIDTH             540
#define DSC_CHUNK_SIZE              540
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               526
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      488
#define DSC_DECREMENT_INTERVAL      7
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          1294
#define DSC_SLICE_BPG_OFFSET        1302
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          3
#define DSC_FLATNESS_MAXQP          12
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    11
#define DSC_RC_QUANT_INCR_LIMIT1    11
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3
#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF
#define MAX_BRIGHTNESS_CLONE 	4095
#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "DCS sequence too big for stack");\
	lcm_dcs_write(ctx, d, ARRAY_SIZE(d));\
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
		ret = lcm_dcs_read(ctx,  0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
#endif
static struct regulator *disp_vddi;
static int lcm_panel_vddi_regulator_init(struct device *dev)
{
	static int vrf18_regulator_inited;
	int ret = 0;
	if (vrf18_regulator_inited)
               return ret;
	/* please only get regulator once in a driver */
	disp_vddi = regulator_get(dev, "vrf18");
	if (IS_ERR(disp_vddi)) { /* handle return value */
		ret = PTR_ERR(disp_vddi);
		pr_err("get disp_vddi fail, error: %d\n", ret);
		return ret;
	}
	vrf18_regulator_inited = 1;
	return ret; /* must be 0 */
}
static unsigned int vrf18_start_up = 1;
static int lcm_panel_vddi_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;
	pr_info("%s +\n",__func__);
	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_vddi, 1800000, 1800000);
	if (ret < 0)
		pr_err("set voltage disp_vddi fail, ret = %d\n", ret);
	retval |= ret;
	status = regulator_is_enabled(disp_vddi);
	pr_info("%s regulator_is_enabled = %d, vrf18_start_up = %d\n", __func__, status, vrf18_start_up);
	if (!status || vrf18_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_vddi);
		if (ret < 0)
			pr_err("enable regulator disp_vddi fail, ret = %d\n", ret);
		vrf18_start_up = 0;
		retval |= ret;
	}
	pr_info("%s -\n",__func__);
	return retval;
}
static int lcm_panel_vddi_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;
	pr_info("%s +\n",__func__);
	status = regulator_is_enabled(disp_vddi);
	pr_info("%s regulator_is_enabled = %d\n", __func__, status);
	if (status){
		ret = regulator_disable(disp_vddi);
		if (ret < 0)
			pr_err("disable regulator disp_vddi fail, ret = %d\n", ret);
	}
	retval |= ret;
	pr_info("%s -\n",__func__);
	return retval;
}
static struct LCM_setting_table lcm_init_setting[] = {
	{0xFF, 1, {0x10}},
	{0xFB, 1, {0x01}},
	{0xB0, 1, {0x00}},
	{0xC1, 16, {0x89,0x28,0x00,0x14,0x02,0x00,0x02,0x0E,0x01,0xE8,0x00,0x07,0x05,0x0E,0x05,0x16}},
	{0xC2, 2, {0x10,0xF0}},
	{0xE9, 1, {0x01}},
	{0xFF, 1, {0xE0}},
	{0xFB, 1, {0x01}},
	{0x35, 1, {0x82}},
	{0xFF, 1, {0xF0}},
	{0xFB, 1, {0x01}},
	{0x1C, 1, {0x01}},
	{0x33, 1, {0x01}},
	{0x5A, 1, {0x00}},
	{0x9F, 1, {0x0E}},
	{0xFF, 1, {0xD0}},
	{0xFB, 1, {0x01}},
	{0x53, 1, {0x22}},
	{0x54, 1, {0x02}},
	{0xFF, 1, {0xC0}},
	{0xFB, 1, {0x01}},
	{0x9C, 1, {0x11}},
	{0x9D, 1, {0x11}},
	{0XFF, 1, {0X25}},
	{0XFB, 1, {0X01}},
	{0x18, 1, {0x22}},
	{0xFF, 1, {0x10}},
	{0xFB, 1, {0x01}},
	/* ESD Setting */
	{0xFF, 1, {0x27} },
	{0xFB, 1, {0x01} },
	{0x40, 1, {0x20} },
	{0xFF, 1, {0x10} },
	{0xFB, 1, {0x01} },
	{0xC0, 1, {0x03} },
	{0x53, 1, {0x24} },
	{0xFF, 1, {0x10} },
	{0xFB, 1, {0x01} },
	{0x35, 1, {0x00} },
	{0x11, 0, {} },
	{REGFLAG_DELAY, 70, {} },
	{0x29, 0, {} },
	{0xFF, 1, {0x27} },
	{0xFB, 1, {0x01} },
	{0x3F, 1, {0x01} },
	{0x43, 1, {0x08} },
	{0x40, 1, {0x25} },
	{0xFF, 1, {0x10} },
	{0xFB, 1, {0x01} },
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};
static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x28, 1, {} },
	{REGFLAG_DELAY, 20, {} },
	{0x10, 1, {} },
	{REGFLAG_DELAY, 60, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};
static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i,j;
	unsigned char temp[255] = {0};
	for (i = 0; i < count; i++) {
		unsigned cmd;
		cmd = table[i].cmd;
		memset(temp, 0, sizeof(temp));
		switch (cmd) {
			case REGFLAG_DELAY:
				if (table[i].count <= 10)
					msleep(table[i].count);
				else
					msleep(table[i].count);
				break;
			case REGFLAG_END_OF_TABLE:
				break;
			default:
				temp[0] = cmd;
				for (j = 0; j < table[i].count; j++) {
					temp[j+1] = table[i].para_list[j];
				}
				lcm_dcs_write(ctx, temp, table[i].count+1);
		}
	}
}
static void lcm_panel_init(struct lcm *ctx)
{
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, (10 * 1000)+20);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(1 * 1000, (1 * 1000)+20);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, (10 * 1000)+20);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	push_table(ctx, lcm_init_setting, sizeof(lcm_init_setting) / sizeof(struct LCM_setting_table));
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
	pr_info("%s+\n", __func__);
	if (!ctx->prepared)
		return 0;
	push_table(ctx, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table));
	ctx->error = 0;
	ctx->prepared = false;

	if (is_tp_doubleclick_enable() == false ||get_panel_dead_flag()) {
		ctx->reset_gpio =
			devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->reset_gpio)) {
			dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
				__func__, PTR_ERR(ctx->reset_gpio));
			return PTR_ERR(ctx->reset_gpio);
		}
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);
		ktz8863a_brightness_set(0);
		ktz8863a_bias_enable(0);
		/* LED EN Power off */
		ctx->leden_gpio =
	   	 devm_gpiod_get_index(ctx->dev, "leden", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->leden_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->leden_gpio);
		udelay(1000);
		/* DVDD Power off */
		ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
			"dvdd", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->dvdd_gpio)) {
			dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
				__func__, PTR_ERR(ctx->dvdd_gpio));
			return PTR_ERR(ctx->dvdd_gpio);
		}
		gpiod_set_value(ctx->dvdd_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
		udelay(1000);
		/* VDDI Power off */
		lcm_panel_vddi_disable(ctx->dev);
	} else {
		/* DVDD Power off */
		ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
			"dvdd", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->dvdd_gpio)) {
			dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
				__func__, PTR_ERR(ctx->dvdd_gpio));
			return PTR_ERR(ctx->dvdd_gpio);
		}
		gpiod_set_value(ctx->dvdd_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);	
	}
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
	if (is_tp_doubleclick_enable() == false ||get_panel_dead_flag()) {
		lcm_panel_vddi_enable(ctx->dev);
		udelay(1000);
		/*VCAM_LDO_EN -------->  GPIO158*/
		ctx->cam_gpio = devm_gpiod_get_index(ctx->dev,
			"cam", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->cam_gpio)) {
			dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
				__func__, PTR_ERR(ctx->cam_gpio));
			return PTR_ERR(ctx->cam_gpio);
		}
		gpiod_set_value(ctx->cam_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->cam_gpio);
		/*DVDD -----> GPIO3*/
		ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
			"dvdd", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->dvdd_gpio)) {
			dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
				__func__, PTR_ERR(ctx->dvdd_gpio));
			return PTR_ERR(ctx->dvdd_gpio);
		}
		gpiod_set_value(ctx->dvdd_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
		udelay(1000);
		/*LED EN Power on*/
		ctx->leden_gpio = devm_gpiod_get_index(ctx->dev,
			"leden", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->leden_gpio)) {
			dev_err(ctx->dev, "%s: cannot get leden_gpio gpio %ld\n",
				__func__, PTR_ERR(ctx->leden_gpio));
			return PTR_ERR(ctx->leden_gpio);
		}
		gpiod_set_value(ctx->leden_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->leden_gpio);
		ktz8863a_bl_bias_conf();
		ktz8863a_bias_enable(1);
	} else {
		/*DVDD -----> GPIO3*/
		ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
			"dvdd", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->dvdd_gpio)) {
			dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
				__func__, PTR_ERR(ctx->dvdd_gpio));
			return PTR_ERR(ctx->dvdd_gpio);
		}
		gpiod_set_value(ctx->dvdd_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
	}
	udelay(10*1000);
	lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);
	ctx->prepared = true;
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif
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
static const struct drm_display_mode mode_30hz = {
	.clock = 326941,
	.hdisplay = 1080,
	.hsync_start = 1080 + 328,			//HFP
	.hsync_end = 1080 + 328 + 10,		//HSA
	.htotal = 1080 + 328 + 10 + 12,		//HBP
	.vdisplay = 2460,
	.vsync_start = 2460 + 5141,			//VFP
	.vsync_end = 2460 + 5141 + 8,		//VSA
	.vtotal = 2460 + 5141 + 8 + 12,	//VBP
};
static const struct drm_display_mode mode_48hz = {
	.clock = 326933,
	.hdisplay = 1080,
	.hsync_start = 1080 + 328,			//HFP
	.hsync_end = 1080 + 328 + 10,		//HSA
	.htotal = 1080 + 328 + 10 + 12,		//HBP
	.vdisplay = 2460,
	.vsync_start = 2460 + 2283,			//VFP
	.vsync_end = 2460 + 2283 + 8,		//VSA
	.vtotal = 2460 + 2283 + 8 + 12,	//VBP
};
static const struct drm_display_mode mode_50hz = {
	.clock = 326898,
	.hdisplay = 1080,
	.hsync_start = 1080 + 328,			//HFP
	.hsync_end = 1080 + 328 + 10,		//HSA
	.htotal = 1080 + 328 + 10 + 12,		//HBP
	.vdisplay = 2460,
	.vsync_start = 2460 + 2092,			//VFP
	.vsync_end = 2460 + 2092 + 8,		//VSA
	.vtotal = 2460 + 2092 + 8 + 12,	//VBP
};
static const struct drm_display_mode default_mode = {
	.clock = 326898,
	.hdisplay = 1080,
	.hsync_start = 1080 + 328,			//HFP
	.hsync_end = 1080 + 328 + 10,		//HSA
	.htotal = 1080 + 328 + 10 + 12,		//HBP
	.vdisplay = 2460,
	.vsync_start = 2460 + 1330,			//VFP
	.vsync_end = 2460 + 1330 + 8,		//VSA
	.vtotal = 2460 + 1330 + 8 + 12,	//VBP
};
static const struct drm_display_mode mode_90hz = {
	.clock = 326898,
	.hdisplay = 1080,
	.hsync_start = 1080 + 328,			//HFP
	.hsync_end = 1080 + 328 + 10,		//HSA
	.htotal = 1080 + 328 + 10 + 12,		//HBP
	.vdisplay = 2460,
	.vsync_start = 2460 + 60,			//VFP
	.vsync_end = 2460 + 60 + 8,		//VSA
	.vtotal = 2460 + 60 + 8 + 12,	//VBP
};
static const struct drm_display_mode mode_120hz = {
	.clock = 375514,
	.hdisplay = 1080,
	.hsync_start = 1080 + 130,			//HFP
	.hsync_end = 1080 + 130 + 10,		//HSA
	.htotal = 1080 + 130 + 10 + 12,		//HBP
	.vdisplay = 2460,
	.vsync_start = 2460 + 60,			//VFP
	.vsync_end = 2460 + 60 + 8,		//VSA
	.vtotal = 2460 + 60 + 8 + 12,	//VBP
};
static const struct drm_display_mode mode_144hz = {
	.clock = 414040,
	.hdisplay = 1080,
	.hsync_start = 1080 + 30,			//HFP
	.hsync_end = 1080 + 30 + 10,		//HSA
	.htotal = 1080 + 30 + 10 + 12,		//HBP
	.vdisplay = 2460,
	.vsync_start = 2460 + 60,			//VFP
	.vsync_end = 2460 + 60 + 8,		//VSA
	.vtotal = 2460 + 60 + 8 + 12,	//VBP
};
#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_30hz = {
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
	.data_rate = DATA_RATE,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
static struct mtk_panel_params ext_params_48hz = {
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
	.data_rate = DATA_RATE,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
static struct mtk_panel_params ext_params_50hz = {
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
	.data_rate = DATA_RATE,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
static struct mtk_panel_params ext_params = {
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
	.data_rate = DATA_RATE,
	.vfp_low_power = 2600,//idle 45hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
	.data_rate = DATA_RATE,
	.vfp_low_power = 1330,//idle 60hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
	.data_rate = DATA_RATE,
	.vfp_low_power = 2600,//idle 60hz
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x21} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
static struct mtk_panel_params ext_params_144hz = {
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 1,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
	.data_rate = DATA_RATE,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 144,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x20} },
		/*switch page for esd check*/
		.dfps_cmd_table[3] = {0, 2, {0xFF, 0x10} },
		.dfps_cmd_table[4] = {0, 2, {0xFB, 0x01} },
	},
	.vfp_low_power = 3620,//idle 60hz
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};
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
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s thh drm_mode_vrefresh = %d, m->hdisplay = %d\n",
		__func__, drm_mode_vrefresh(m), m->hdisplay);
	if (drm_mode_vrefresh(m) == 30)
		ext->params = &ext_params_30hz;
	else if (drm_mode_vrefresh(m) == 48)
		ext->params = &ext_params_48hz;
	else if (drm_mode_vrefresh(m) == 50)
		ext->params = &ext_params_50hz;
	else if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == 120)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == 144)
		ext->params = &ext_params_144hz;
	else
		ret = 1;
	if (!ret)
		ctx->dynamic_fps = drm_mode_vrefresh(m);
	return ret;
}
static void mode_switch_to_144(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s\n", __func__);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
}
static void mode_switch_to_120(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s\n", __func__);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
}
static void mode_switch_to_90(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s\n", __func__);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x22);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
}
static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);
	if (drm_mode_vrefresh(m) == 90) {
		mode_switch_to_90(panel);
	} else if (drm_mode_vrefresh(m) == 120) {
		mode_switch_to_120(panel);
	} else if (drm_mode_vrefresh(m) == 144) {
		mode_switch_to_144(panel);
	} else
		ret = 1;
	if(!ret)
		ctx->dynamic_fps = drm_mode_vrefresh(m);
	return ret;
}
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	return 0;
}
#ifdef CONFIG_MI_DISP
static u8 read_wp_info[8] = {0};
int panel_trigger_get_wpinfo(struct drm_panel *panel, char *buf, size_t size)
{
        /*      Read 3 bytes from 0xF2 register
	 * 	Bytes[0] = Wx
	 * 	Bytes[1] = Wy
	 * 	Bytes[2] = Lux <Max_brightness>
	 */
	int ret = 0;
	int i = 0;
	char select_page_cmd1[] = {0xFF, 0x21};
	char select_page_cmd2[] = {0xFF, 0x10};
	u8 tx[10] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto NOMEM;
	}
	/* CMD2 Page1 is selected */
	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!cmd_msg)
		goto NOMEM;
	cmd_msg->channel = 1;
	cmd_msg->flags = 2;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = ARRAY_SIZE(select_page_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg->tx_buf[0] = select_page_cmd1;
	cmd_msg->tx_len[0] = ARRAY_SIZE(select_page_cmd1);
	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE2;
	}
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
	/* Read the first 3 bytes of CMD2 page1 0xF2 reg */
	cmd_msg->channel = 0;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = 0x06;
	tx[0] = 0xF2;
	cmd_msg->tx_buf[0] = tx;
	cmd_msg->tx_len[0] = 1;
	cmd_msg->rx_cmd_num = 1;
	cmd_msg->rx_buf[0] = kzalloc(3 * sizeof(unsigned char), GFP_KERNEL);
	if (!cmd_msg->rx_buf[0]) {
		pr_err("%s: memory allocation failed\n", __func__);
		ret = 0;
		goto DONE2;
	}
	cmd_msg->rx_len[0] = 3;
	ret = mtk_ddic_dsi_read_cmd(cmd_msg);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE1;
	}
	for (i = 0; i < 3; i++) {
		pr_info("read reg value:0x%x--byte:%d,val:0x%02hhx\n",
			*(unsigned char *)(cmd_msg->tx_buf[0]), i,
			*(unsigned char *)(cmd_msg->rx_buf[0] + i));
	}
	read_wp_info[0] = *(unsigned char *)cmd_msg->rx_buf[0];
	read_wp_info[1] = *(unsigned char *)cmd_msg->rx_buf[1];
	read_wp_info[2] = *(unsigned char *)cmd_msg->rx_buf[2];
	/*CMD2 Page1 is selected*/
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
	cmd_msg->channel = 1;
	cmd_msg->flags = 2;
	cmd_msg->tx_cmd_num = 1;
	cmd_msg->type[0] = ARRAY_SIZE(select_page_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg->tx_buf[0] = select_page_cmd2;
	cmd_msg->tx_len[0] = ARRAY_SIZE(select_page_cmd2);
	mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
DONE1:
	kfree(cmd_msg->rx_buf[0]);
DONE2:
	vfree(cmd_msg);
	pr_info("%s end -\n", __func__);
	return ret;
NOMEM:
	pr_err("%s: memory allocation failed\n", __func__);
	return 0;
}
int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	int ret = 0;
	if (sscanf(led_wp_info_str, "0x%02hhx, 0x%02hhx, 0x%02hhx",
		&read_wp_info[0], &read_wp_info[1], &read_wp_info[2]) == 3) {
		if (read_wp_info[0] == 0 && read_wp_info[1] == 0 && read_wp_info[2] == 0) {
			pr_err("No panel is Connected !");
			goto err;
		}
		ret = snprintf(buf, size, "%02hhx%02hhx%02hhx\n",
			read_wp_info[0], read_wp_info[1], read_wp_info[2]);
		pr_info("read maxlum & wp: %02hhx, %02hhx, %02hhx\n",
			read_wp_info[0], read_wp_info[1], read_wp_info[2]);
	} else {
		pr_err("failed to parse wp and lux info from cmdline !\n");
		ret = snprintf(buf, size, "%02hhx%02hhx%02hhx\n",
			read_wp_info[0], read_wp_info[1], read_wp_info[2]);
		pr_info("read maxlum & wp: %02hhx, %02hhx, %02hhx\n",
			read_wp_info[0], read_wp_info[1], read_wp_info[2]);
	}
	return ret;
err:
	return 0;
}
static unsigned int bl_level = 2047;
static int lcm_setbacklight_i2c(struct drm_panel *panel, unsigned int level)
{
	struct lcm *ctx = panel_to_lcm(panel);
	if (level)
		ctx->prepared = true;
#ifdef CONFIG_FACTORY_BUILD
	if ((level >= 0 && level < 2047) || (ctx->hbm_enabled == false && (level == 2047)))
	{
		bl_level = level;
		ktz8863a_brightness_set(level);
	}
	if (ctx->hbm_enabled)
		ctx->hbm_enabled = false;
#else
	bl_level = level;
	ktz8863a_brightness_set(level);
#endif
	return level;
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
static int lcm_led_i2c_reg_op(char *buffer, int op, int count)
{
	int i, ret = -EINVAL;
	char reg_addr = *buffer;
	char *reg_val = buffer;
	if (reg_val == NULL) {
		pr_err("%s,buffer is null\n", __func__);
		return ret;
	}
	pr_info("%s,reg_val is %s reg_addr = %s\n", __func__, buffer, reg_addr);
	pr_info("pr_info%s,reg_val is %s reg_addr = %s\n", __func__, buffer, reg_addr);
	if (op == KTZ8863A_REG_READ) {
		for (i = 0; i < count; i++) {
			ret = ktz8863a_reg_read_bytes(reg_addr, reg_val);
			if (ret <= 0)
				break;
			reg_addr++;
			reg_val++;
		}
	} else if (op == KTZ8863A_REG_WRITE) {
		ret = ktz8863a_reg_write_bytes(reg_addr, *(reg_val + 1));
	}
	return ret;
}

static int panel_get_dynamic_fps(struct drm_panel *panel, u32 *fps)
{
	int ret = 0;
	struct lcm *ctx;
	if (!panel || !fps) {
		pr_err("%s: panel or fps is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);
	*fps = ctx->dynamic_fps;
err:
	return ret;
}
static int panel_get_max_brightness_clone(struct drm_panel *panel, u32 *max_brightness_clone)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	*max_brightness_clone = ctx->max_brightness_clone;
	return 0;
}
static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level)
{
	struct lcm *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = panel_to_lcm(panel);
	if (level > 2047) {
		ctx->hbm_enabled = true;
		bl_level = level;
		ktz8863a_brightness_set(level);
	}
	pr_err("lcm_setbacklight_control backlight %d\n", level);
	return 0;
}
#endif
static bool get_lcm_initialized(struct drm_panel *panel)
{
	struct lcm *ctx;
	bool ret = false;
	if (!panel) {
		pr_err(": panel is NULL\n", __func__);
		goto err;
	}
	ctx = panel_to_lcm(panel);
	ret = ctx->prepared;
err:
	return ret;
}
static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
#ifdef CONFIG_MI_DISP
	.set_backlight_i2c = lcm_setbacklight_i2c,
	.get_panel_info = panel_get_panel_info,
	.led_i2c_reg_op = lcm_led_i2c_reg_op,
	.get_panel_dynamic_fps =  panel_get_dynamic_fps,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
	.trigger_get_wpinfo = panel_trigger_get_wpinfo,
	.get_wp_info = panel_get_wp_info,
	.setbacklight_control = lcm_setbacklight_control,
	.get_panel_initialized = get_lcm_initialized,
#endif
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
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};
static int lcm_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct drm_display_mode *mode_30;
	struct drm_display_mode *mode_48;
	struct drm_display_mode *mode_50;
	struct drm_display_mode *mode_60;
	struct drm_display_mode *mode_90;
	struct drm_display_mode *mode_120;
	struct drm_display_mode *mode_144;
	mode_30 = drm_mode_duplicate(connector->dev, &mode_30hz);
	if (!mode_30) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_30hz.hdisplay, mode_30hz.vdisplay,
			drm_mode_vrefresh(&mode_30hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_30);
	mode_30->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_30);
	mode_48 = drm_mode_duplicate(connector->dev, &mode_48hz);
	if (!mode_48) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_48hz.hdisplay, mode_48hz.vdisplay,
			drm_mode_vrefresh(&mode_48hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_48);
	mode_48->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_48);
	mode_50 = drm_mode_duplicate(connector->dev, &mode_50hz);
	if (!mode_50) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_50hz.hdisplay, mode_50hz.vdisplay,
			drm_mode_vrefresh(&mode_50hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_50);
	mode_50->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_50);
	mode_60 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode_60) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_60);
	mode_60->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_60);
	mode_90 = drm_mode_duplicate(connector->dev, &mode_90hz);
	if (!mode_90) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_90hz.hdisplay, mode_90hz.vdisplay,
			drm_mode_vrefresh(&mode_90hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90);
	mode_90->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_90);
	mode_120 = drm_mode_duplicate(connector->dev, &mode_120hz);
	if (!mode_120) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_120hz.hdisplay, mode_120hz.vdisplay,
			drm_mode_vrefresh(&mode_120hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_120);
	mode_120->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_120);
	mode_144 = drm_mode_duplicate(connector->dev, &mode_144hz);
	if (!mode_144) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_144hz.hdisplay, mode_144hz.vdisplay,
			drm_mode_vrefresh(&mode_144hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_144);
	mode_144->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_144);
	connector->display_info.width_mm = PHYSICAL_WIDTH/1000;
	connector->display_info.height_mm = PHYSICAL_HEIGHT/1000;
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
	pr_info("l16_42_02_0a_dsc_vdo %s+\n", __func__);
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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 |MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET;
	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);
		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
	ctx->leden_gpio = devm_gpiod_get(dev, "leden", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->leden_gpio)) {
		dev_err(dev, "%s: cannot get leden_gpios %ld\n",
			__func__, PTR_ERR(ctx->leden_gpio));
		return PTR_ERR(ctx->leden_gpio);
	}
	devm_gpiod_put(dev, ctx->leden_gpio);
	ctx->dvdd_gpio = devm_gpiod_get_index(dev, "dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(dev, "%s: cannot get dvdd_gpio 0 %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	devm_gpiod_put(dev, ctx->dvdd_gpio);
	ret = lcm_panel_vddi_regulator_init(dev);
	if (!ret)
		lcm_panel_vddi_enable(dev);
	else
		pr_err("%s init vrf18_aif regulator error\n", __func__);
	ext_params.err_flag_irq_gpio = of_get_named_gpio_flags(
		dev->of_node, "mi,esd-err-irq-gpio",
		0, (enum of_gpio_flags *)&(ext_params.err_flag_irq_flags));
	ext_params_30hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_30hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_48hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_48hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_50hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_50hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_90hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_90hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_120hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_144hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_144hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ctx->prepared = true;
	ctx->enabled = true;
	ctx->hbm_enabled = false;
	ctx->panel_info = panel_name;
	ctx->dynamic_fps = 60;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;
	drm_panel_add(&ctx->panel);
	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);
#if defined(CONFIG_MTK_PANEL_EXT)
	//mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	pr_info("l16_42_02_0a_dsc_vdo %s-\n", __func__);
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
	{ .compatible = "l16_42_02_0a_dsc_vdo,lcm", },
	{ }
};
MODULE_DEVICE_TABLE(of, lcm_of_match);
static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "l16_42_02_0a_dsc_vdo,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};
module_param_string(WpAndMaxlum, led_wp_info_str, sizeof(led_wp_info_str), 0600);
MODULE_PARM_DESC(WpAndMaxlum, "panel-l16-42-02-0a-dsc-vdo.WpAndMaxlum=<WpAndMaxlum> while <WpAndMaxlum> is 'WpAndMaxlum' ");
module_mipi_dsi_driver(lcm_driver);
MODULE_AUTHOR("Lang Lei <leilang1@xiaomi.com>");
MODULE_DESCRIPTION("L16 42 02 0a dsc vdo lcd panel driver");
MODULE_LICENSE("GPL v2");
