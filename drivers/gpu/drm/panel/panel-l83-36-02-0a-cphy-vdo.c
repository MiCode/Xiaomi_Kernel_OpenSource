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
#ifdef CONFIG_MI_DISP
#include <uapi/drm/mi_disp.h>
#endif
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#include "../mediatek/mediatek_v2/mtk_drm_crtc.h"
#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "mi_dsi_panel.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
static const char *panel_name = "panel_name=dsi_l83_36_02_0a_cphy_vdo";

#define MAX_CMDLINE_PARAM_LEN 64
static char led_wp_info_str[64] = {0};
struct lcm *ctx_panel;

extern int ktz8866_bl_bias_conf_36(void);
extern int ktz8866_bias_enable(int enable, int delayMs);
extern int ktz8866_brightness_set(int level);
extern int ktz8866_reg_write_bytes(unsigned char addr, unsigned char value);
extern int ktz8866_reg_read_bytes(unsigned char addr, char *value);

#define PHYSICAL_WIDTH              138600
#define PHYSICAL_HEIGHT             231000

#define DATA_RATE                   970

#define FRAME_WIDTH                 1200
#define HSA                         10
#define HBP                         40
#define HFP                         60

#define FRAME_HEIGHT                2000
#define VSA                         4
#define VBP                         4
#define VFP_90                      26
#define VFP_60                      1042

#define FPS                         90

#define DSC_ENABLE                  0

#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF
#define MAX_BRIGHTNESS_CLONE 	8191

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

/*  |---KTZ8866_EN--->BK_HW--->GPIO12   */
/*      |---AVDD-->	VSP +5V --> KTZ8866_VREG_DISP_P(GPIO150/bias0)*/
/*      |---AVEE-->	VSN	 -5V --> KTZ8866_VREG_DISP_N(GPIO151/bias0)*/
/*  |---LCD_VDDIO_1P8	-->	LCD_1V8   --> VIO18_PMU */
static struct regulator *disp_vddi;
static int lcm_panel_vddi_regulator_init(struct device *dev)
{
	static int vio18_regulator_inited;
	int ret = 0;

	if (vio18_regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_vddi = regulator_get(dev, "vio18");
	if (IS_ERR(disp_vddi)) { /* handle return value */
		ret = PTR_ERR(disp_vddi);
		disp_vddi = NULL;
		pr_err("get disp_vddi fail, error: %d\n", ret);
		return ret;
	}

	vio18_regulator_inited = 1;
	return ret; /* must be 0 */
}

static unsigned int vio18_start_up = 1;
static int lcm_panel_vddi_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_info("%s +\n",__func__);
	if (disp_vddi == NULL)
		return retval;

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_vddi, 1800000, 1800000);
	if (ret < 0)
		pr_err("set voltage disp_vddi fail, ret = %d\n", ret);
	retval |= ret;

	status = regulator_is_enabled(disp_vddi);
	pr_info("%s regulator_is_enabled = %d, vio18_start_up = %d\n", __func__, status, vio18_start_up);
	if (!status || vio18_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_vddi);
		if (ret < 0)
			pr_err("enable regulator disp_vddi fail, ret = %d\n", ret);
		vio18_start_up = 0;
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
	if (disp_vddi == NULL)
		return retval;

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
	{0xFF, 1,  {0xD0} },
	{0xFB, 1,  {0x01} },
	{0x09, 1,  {0xEE} },
	{0x1C, 1,  {0x88} },
	{0x1D, 1,  {0x08} },

	{0xFF, 1,  {0xE0} },
	{0xFB, 1,  {0x01} },
	{0x14, 1,  {0x60} },
	{0x16, 1,  {0xC0} },

	{0xFF, 1,  {0xF0} },
	{0xFB, 1,  {0x01} },
	{0x3A, 1,  {0x08} },

	{0xFF, 1,  {0x27} },
	{0xFB, 1,  {0x01} },
	{0xD0, 1,  {0x31} },
	{0xD1, 1,  {0x20} },
	{0xD4, 1,  {0x08} },
	{0xDE, 1,  {0x80} },
	{0xDF, 1,  {0x02} },

	{0xFF, 1,  {0x10} },
	{0xFB, 1,  {0x01} },
	{0xB9, 1,  {0x01} },

	{0xFF, 1,  {0x20} },
	{0xFB, 1,  {0x01} },
	{0x18, 1,  {0x40} },

	{0xFF, 1,  {0x10} },
	{0xFB, 1,  {0x01} },
	{0xB9, 1,  {0x02} },
	{0xBB, 1,  {0x13} },
	{0xB9, 1,  {0x02} },
	{0x3B, 5,  {0x03,0x08,0x1A,0x04,0x04} },

	{0x35, 1,  {0x00} },

	{0x51, 2,  {0x00,0x00}},

	{0x11, 0, {} },
	{REGFLAG_DELAY, 120, {} },
	{0x29, 0, {} },
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
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	push_table(ctx, lcm_init_setting, sizeof(lcm_init_setting) / sizeof(struct LCM_setting_table));
	schedule_delayed_work(&ctx->esd_enable_delayed_work, msecs_to_jiffies(100));
	pr_info("%s-\n", __func__);
}

static void panel_esd_enable_delayed_work(struct work_struct *work)
{
	char esd_write_cmd0[] = {0xFF, 0x27};
	char esd_write_cmd1[] = {0xFB, 0x01};
	char esd_write_cmd2[] = {0xD1, 0x28};
	char esd_write_cmd3[] = {0xFF, 0x10};
	char esd_write_cmd4[] = {0xFB, 0x01};
	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 5,
 	};
	cmd_msg.type[0] = 0x15;
	cmd_msg.tx_buf[0] = esd_write_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(esd_write_cmd0);
	cmd_msg.type[1] = 0x15;
	cmd_msg.tx_buf[1] = esd_write_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(esd_write_cmd1);
	cmd_msg.type[2] = 0x15;
	cmd_msg.tx_buf[2] = esd_write_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(esd_write_cmd2);
	cmd_msg.type[3] = 0x15;
	cmd_msg.tx_buf[3] = esd_write_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(esd_write_cmd3);
	cmd_msg.type[4] = 0x15;
	cmd_msg.tx_buf[4] = esd_write_cmd4;
	cmd_msg.tx_len[4] = ARRAY_SIZE(esd_write_cmd4);
	mtk_ddic_dsi_send_cmd(&cmd_msg, false, false);

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

	if (is_tp_doubleclick_enable() == false) {
		ctx->reset_gpio =
			devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->reset_gpio)) {
			dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
				__func__, PTR_ERR(ctx->reset_gpio));
			return PTR_ERR(ctx->reset_gpio);
		}
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		ktz8866_brightness_set(0);
		ktz8866_bias_enable(0, 5);
		/* AVDD&AVEE Power off */
		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		ctx->bias_neg =
			devm_gpiod_get_index(ctx->dev, "bias", 1, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		/* KTZ8866 EN Power off */
		ctx->ktzen_gpio = devm_gpiod_get_index(ctx->dev,
			"ktzen", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->ktzen_gpio)) {
			dev_err(ctx->dev, "%s: cannot get ktzen gpio %ld\n",
				__func__, PTR_ERR(ctx->ktzen_gpio));
			return PTR_ERR(ctx->ktzen_gpio);
		}
		gpiod_set_value(ctx->ktzen_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->ktzen_gpio);

		/* VDDI Power off */
		lcm_panel_vddi_disable(ctx->dev);
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

	if (is_tp_doubleclick_enable() == false) {
		/*LCD_VDDIO_1P8	-->	LCD_1V8   --> VIO18_PMU*/
		lcm_panel_vddi_enable(ctx->dev);
		udelay(1000);

		/*
			//Backup 1V8 GPIO153 -------->  GPIO153
			ctx->cam_gpio = devm_gpiod_get_index(ctx->dev,
				"cam", 0, GPIOD_OUT_HIGH);
			if (IS_ERR(ctx->cam_gpio)) {
				dev_err(ctx->dev, "%s: cannot get cam_gpio gpio %ld\n",
					__func__, PTR_ERR(ctx->cam_gpio));
				return PTR_ERR(ctx->cam_gpio);
			}
			gpiod_set_value(ctx->cam_gpio, 1);
			devm_gpiod_put(ctx->dev, ctx->cam_gpio);
		*/

		/*KTZ8866 EN -----> GPIO12--->BK_HW*/
		ctx->ktzen_gpio = devm_gpiod_get_index(ctx->dev,
			"ktzen", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->ktzen_gpio)) {
			dev_err(ctx->dev, "%s: cannot get ktzen gpio %ld\n",
				__func__, PTR_ERR(ctx->ktzen_gpio));
			return PTR_ERR(ctx->ktzen_gpio);
		}
		gpiod_set_value(ctx->ktzen_gpio, 1);
		devm_gpiod_put(ctx->dev, ctx->ktzen_gpio);
		udelay(1000);

		/*|---KTZ8866_ENP--->GPIO150 */
		ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
			"bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_err(ctx->dev, "%s: cannot get bias_pos gpio %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		/*|---KTZ8866_ENN--->GPIO151 */
		ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
			"bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_err(ctx->dev, "%s: cannot get bias_neg gpio %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		/* AVDD&AVEE ±5V Power on*/
		ktz8866_bl_bias_conf_36();
		ktz8866_bias_enable(1, 1);
	} else {
		gpiod_set_value(ctx->reset_gpio, 0);
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



static const struct drm_display_mode default_mode = {
	.clock = 240768,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,		//HFP
	.hsync_end = FRAME_WIDTH + HFP + HSA,		//HSA
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,	//HBP
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + VFP_90,		//VFP
	.vsync_end = FRAME_HEIGHT + VFP_90 + VSA,	//VSA
	.vtotal = FRAME_HEIGHT + VFP_90 + VSA + VBP,	//VBP
};

static const struct drm_display_mode mode_60hz = {
	.clock = 240768,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + HFP,		//HFP
	.hsync_end = FRAME_WIDTH + HFP + HSA,		//HSA
	.htotal = FRAME_WIDTH + HFP + HSA + HBP,	//HBP
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + VFP_60,		//VFP
	.vsync_end = FRAME_HEIGHT + VFP_60 + VSA,	//VSA
	.vtotal = FRAME_HEIGHT + VFP_60 + VSA + VBP,	//VBP
};

#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params = {
	.pll_clk = DATA_RATE / 2,
	.is_cphy = 1,
	.ssc_enable = 1,
	.output_mode = MTK_PANEL_SINGLE_PORT,
	.data_rate = DATA_RATE,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size=sizeof(top_rc_pattern),
	.corner_pattern_lt_addr=(void*)top_rc_pattern,
#endif
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.phy_timcon = {
		.hs_trail = 32,
	},
};

static struct mtk_panel_params ext_params_60hz = {
	.pll_clk = DATA_RATE / 2,
	.is_cphy = 1,
	.ssc_enable = 1,
	.output_mode = MTK_PANEL_SINGLE_PORT,
	.data_rate = DATA_RATE,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size=sizeof(top_rc_pattern),
	.corner_pattern_lt_addr=(void*)top_rc_pattern,
#endif
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.phy_timcon = {
		.hs_trail = 32,
	},
};

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;
	pr_info("%s+ mode = %d\n", __func__, mode);

	list_for_each_entry(m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
	struct drm_connector *connector,
	struct mtk_panel_params **ext_param,
	unsigned int mode)
{
	int ret = 0;
	pr_info("%s+ mode = %d\n", __func__, mode);

	if (mode == 0)
		*ext_param = &ext_params;
	else if (mode == 1)
		*ext_param = &ext_params_60hz;
	else
		ret = 1;

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);
	struct lcm *ctx = panel_to_lcm(panel);
#ifdef CONFIG_MI_DISP_NOTIFIER
	struct mi_disp_notifier g_notify_data;
	int blank;
#endif
	pr_info("%s+ drm_mode_vrefresh = %d, m->hdisplay = %d\n",
		__func__, drm_mode_vrefresh(m), m->hdisplay);

	if (drm_mode_vrefresh(m) == 90)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == 60)
		ext->params = &ext_params_60hz;
	else
		ret = 1;
	if (!ret){
		ctx->dynamic_fps = drm_mode_vrefresh(m);
#ifdef CONFIG_MI_DISP_NOTIFIER
		blank = ctx->dynamic_fps;
		g_notify_data.data = &blank;
		g_notify_data.disp_id = MI_DISPLAY_PRIMARY;
		mi_disp_notifier_call_chain(MI_DISP_CHANGE_FPS, &g_notify_data);
		mi_disp_feature_event_notify_by_type(mi_get_disp_id("primary"), MI_DISP_EVENT_FPS, sizeof(blank), blank);
#endif
	}

	return ret;
}

static void mode_switch_to_60(struct drm_panel *panel)
{
	// struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

}

static void mode_switch_to_90(struct drm_panel *panel)
{
	// struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s mode_switch+ cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	if (drm_mode_vrefresh(m) == 90) {
		mode_switch_to_90(panel);
	} else if (drm_mode_vrefresh(m) == 60) {
		mode_switch_to_60(panel);
	} else
		ret = 1;
	if(!ret)
		ctx->dynamic_fps = drm_mode_vrefresh(m);
	pr_info("%s mode_switch- cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	return ret;
}

static unsigned int bl_level = 2047;
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int level)
{
	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;
	struct drm_panel *panel = mtk_dsi->panel;
	struct lcm *ctx;
	static uint16_t last_bl_level= 0;
	char dsi_disable_insert_black_cmd[] = {0x13, 0x00};
	char dsi_insert_black_cmd[] = {0x22, 0x00};

	if (!panel) {
		pr_err(": panel is NULL\n", __func__);
		return -1;
	}
	ctx = panel_to_lcm(panel);

	if (level==0&&last_bl_level!=0){
		mutex_lock(&ctx->panel_lock);
		cb(dsi, handle, dsi_insert_black_cmd, ARRAY_SIZE(dsi_insert_black_cmd));
		mutex_unlock(&ctx->panel_lock);
		usleep_range((18* 1000), (18* 1000)+10);
	}else if (level!=0&&last_bl_level==0){
		mutex_lock(&ctx->panel_lock);
		cb(dsi, handle, dsi_disable_insert_black_cmd, ARRAY_SIZE(dsi_disable_insert_black_cmd));
		mutex_unlock(&ctx->panel_lock);
	}

	bl_level = level;
	ktz8866_brightness_set(level);
	last_bl_level =  level;
	return 0;
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
int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	/* 	Read 3 bytes from 0x23 0x24 0x25 register
	 * 	0x23 = Wx
	 * 	0x24 = Wy
	 * 	0x25 = Lux <Max_brightness>
	 */
	int ret = 0;
	int i = 0;
	char select_page_cmd1[] = {0xFF, 0x20};
	char select_page_cmd2[] = {0xFF, 0x10};
	u8 wp_cmd[] ={0x23,0x24,0x25};
	u8 wp_buf[8] = {0};
	struct mtk_ddic_dsi_msg *cmd_msg;

	struct mtk_ddic_dsi_msg cmds[] = {
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &wp_cmd[0],
			.tx_len[0] = 1,
			.rx_buf[0] = &wp_buf[0],
			.rx_len[0] = 1,
		},
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &wp_cmd[1],
			.tx_len[0] = 1,
			.rx_buf[0] = &wp_buf[1],
			.rx_len[0] = 1,
		},
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &wp_cmd[2],
			.tx_len[0] = 1,
			.rx_buf[0] = &wp_buf[2],
			.rx_len[0] = 1,
		}
	};

	if (sscanf(led_wp_info_str, "0x%02hhx, 0x%02hhx, 0x%02hhx",
		&wp_buf[0], &wp_buf[1], &wp_buf[2])!= 4) {
		pr_err("failed to parse wp and lux info from cmdline !\n");
	} else {
		ret = snprintf(buf, size, "%02x%02x%02x\n",
			wp_buf[0], wp_buf[1], wp_buf[2]);
		pr_info("read maxlum & wp: %hhu, %hhu, %hhu\n",
			wp_buf[0], wp_buf[1], wp_buf[2]);
		goto DONE2;
	}

	/* CMD2 Page1 is selected */
	pr_info("%s start +\n", __func__);

	cmd_msg = vmalloc(sizeof(struct mtk_ddic_dsi_msg));
	if (!cmd_msg)
		goto NOMEM;

	cmd_msg->channel = 1;
	cmd_msg->flags = 2;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = select_page_cmd1;
	cmd_msg->tx_len[0] = 2;

	ret = mtk_ddic_dsi_send_cmd(cmd_msg, true, false);
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE1;
	}

	/* Read the first  bytes of CMD2 page1 0x23 0x24 0x25 reg */
	memset(wp_buf, 0, sizeof(wp_buf));
	for (i = 0; i < sizeof(cmds)/sizeof(struct mtk_ddic_dsi_msg); ++i) {
		ret |= mtk_ddic_dsi_read_cmd(&cmds[i]);
	}
	if (ret != 0) {
		pr_err("%s error\n", __func__);
		goto DONE1;
	}
	pr_info("read reg value:%hhu, %hhu, %hhu\n", wp_buf[0], wp_buf[1], wp_buf[2]);
	ret = snprintf(buf, size, "%02hhx%02hhx%02hhx\n", wp_buf[0], wp_buf[1], wp_buf[2]);

	/*CMD2 Page1 is selected*/
	memset(cmd_msg, 0, sizeof(struct mtk_ddic_dsi_msg));
	cmd_msg->channel = 1;
	cmd_msg->flags = 2;
	cmd_msg->tx_cmd_num = 1;

	cmd_msg->type[0] = 0x15;
	cmd_msg->tx_buf[0] = select_page_cmd2;
	cmd_msg->tx_len[0] = 2;

	mtk_ddic_dsi_send_cmd(cmd_msg, true, false);

DONE1:
	vfree(cmd_msg);

	pr_info("%s end -\n", __func__);
	return ret;
DONE2:
	return ret;

NOMEM:
	pr_err("%s: memory allocation failed\n", __func__);
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

static int lcm_led_i2c_reg_op(char *buffer, int op, int count)
{
	int i, ret = -EINVAL;
	char reg_addr = *buffer;
	char *reg_val = buffer;
	if (reg_val == NULL) {
		pr_err("%s,buffer is null\n", __func__);
		return ret;
	}

	if (op == KTZ8866_REG_READ) {
		for (i = 0; i < count; i++) {
			ret = ktz8866_reg_read_bytes(reg_addr, reg_val);
			pr_info("read ktz8866 reg_addr 0x%02x is 0x%02x\n", reg_addr, *(reg_val));
			if (ret <= 0)
				break;
			reg_addr++;
			reg_val++;
		}
	} else if (op == KTZ8866_REG_WRITE) {
		ret = ktz8866_reg_write_bytes(reg_addr, *(reg_val + 1));
		pr_info("write ktz8866 reg_addr 0x%02x is 0x%02x\n", reg_addr, *(reg_val + 1));
	}

	return 0;
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

static int panel_get_panel_thermal_dimming_enable(struct drm_panel *panel, bool *enabled)
{
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	*enabled = true;

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
		ktz8866_brightness_set(level);
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
	.ext_param_get = mtk_panel_ext_param_get,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
#ifdef CONFIG_MI_DISP
	.get_panel_info = panel_get_panel_info,
	.led_i2c_reg_op = lcm_led_i2c_reg_op,
	.get_panel_dynamic_fps =  panel_get_dynamic_fps,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
	.get_panel_thermal_dimming_enable = panel_get_panel_thermal_dimming_enable,
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
	struct drm_display_mode *mode_60;
	struct drm_display_mode *mode_90;

	mode_60 = drm_mode_duplicate(connector->dev, &mode_60hz);
	if (!mode_60) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_60hz.hdisplay, mode_60hz.vdisplay,
			drm_mode_vrefresh(&mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_60);
	mode_60->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_60);

	mode_90 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode_90) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_90);
	mode_90->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode_90);

	connector->display_info.width_mm = PHYSICAL_WIDTH/1000;
	connector->display_info.height_mm = PHYSICAL_HEIGHT/1000;
	pr_info("lcm_get_modes %s +/-\n", __func__);
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

	pr_info("l83_36_02_0a_cphy_vdo %s +\n", __func__);

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
	dsi->lanes = 3;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET |
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
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(dev, "cannot get bias-gpios 0 %ld\n",
			 PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_neg)) {
		dev_info(dev, "cannot get bias-gpios 1 %ld\n",
			 PTR_ERR(ctx->bias_neg));
		return PTR_ERR(ctx->bias_neg);
	}
	devm_gpiod_put(dev, ctx->bias_neg);

	ctx->ktzen_gpio = devm_gpiod_get_index(dev, "ktzen", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->ktzen_gpio)) {
		dev_err(dev, "%s: cannot get ktzen_gpio 0 %ld\n",
			__func__, PTR_ERR(ctx->ktzen_gpio));
		return PTR_ERR(ctx->ktzen_gpio);
	}
	devm_gpiod_put(dev, ctx->ktzen_gpio);

	ret = lcm_panel_vddi_regulator_init(dev);
	if (!ret)
		lcm_panel_vddi_enable(dev);
	else
		pr_err("%s init vio18_aif regulator error\n", __func__);

	ext_params.err_flag_irq_gpio = of_get_named_gpio_flags(
			dev->of_node, "mi,esd-err-irq-gpio",
			0, (enum of_gpio_flags *)&(ext_params.err_flag_irq_flags));

	ext_params_60hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_60hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->hbm_enabled = false;
	ctx->panel_info = panel_name;
	ctx->dynamic_fps = 90;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;

	INIT_DELAYED_WORK(&ctx->esd_enable_delayed_work, panel_esd_enable_delayed_work);

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
	pr_info("l83_36_02_0a_cphy_vdo %s-\n", __func__);

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
	{ .compatible = "l83_36_02_0a_cphy_vdo,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "l83_36_02_0a_cphy_vdo,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_param_string(WpAndMaxlum, led_wp_info_str, MAX_CMDLINE_PARAM_LEN, 0600);
MODULE_PARM_DESC(WpAndMaxlum, "panel-l83-36-02-0a-cphy-vdo.WpAndMaxlum=<WpAndMaxlum> while <WpAndMaxlum> is 'WpAndMaxlum' ");

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Teng Zhang <zhangteng3@xiaomi.com>");
MODULE_DESCRIPTION("L83 36 02 0a chpy vdo lcd panel driver");
MODULE_LICENSE("GPL v2");
