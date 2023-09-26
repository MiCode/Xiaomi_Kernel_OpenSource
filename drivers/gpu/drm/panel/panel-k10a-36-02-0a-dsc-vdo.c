/*
 * Copyright (c) 2015 MediaTek Inc.
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

#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>


#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <mt-plat/upmu_common.h>
#include <linux/double_click.h>
#include "../mediatek/mi_disp/mi_disp_feature_id.h"
#include "../mediatek/mi_disp/mi_dsi_panel_count.h"

extern int lm36273_bl_bias_conf(void);
extern int lm36273_bias_enable(int enable, int delayMs);
extern int lm36273_brightness_set(int level);
extern int lm36273_reg_read_bytes(char addr, char *buf);
extern int lm36273_reg_write_bytes(unsigned char addr, unsigned char value);
extern int hbm_brightness_set(uint32_t level);

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mtk_drm_graphics_base.h"
#include "../mediatek/mtk_log.h"
#include "../mediatek/mtk_panel_ext.h"
#endif

#if 0
#include "../mediatek/dsi_panel_mi_count.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern_k10a_l.h"
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern_k10a_r.h"
#endif
/* enable this to check panel self -bist pattern */
/* #define PANEL_BIST_PATTERN */
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
//#include "lcm_i2c.h"

#define HFP_SUPPORT 1

#if HFP_SUPPORT
static int current_fps = 60;
#endif

extern int get_panel_dead_flag(void);

/* i2c control start */
#define LCM_I2C_ID_NAME "I2C_LCD_DVDD"
//static struct i2c_client *_lcm_i2c_client;

#define REGFLAG_DELAY               0xFFFC
#define REGFLAG_UDELAY              0xFFFB
#define REGFLAG_END_OF_TABLE        0xFFFD
#define REGFLAG_RESET_LOW           0xFFFE
#define REGFLAG_RESET_HIGH          0xFFFF
#define PANEL_CLOCK                 414831

#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2400

#define PHYSICAL_WIDTH              70200
#define PHYSICAL_HEIGHT             152100

#define DATA_RATE                   834
#define HSA                         10
#define HBP                         8
#define VSA                         10
#define VBP                         10

#define LP_MODE0_VFP                549
#define LP_MODE0_FPS                50

#define LP_MODE1_VFP                673
#define LP_MODE1_FPS                48

#define LP_MODE2_VFP                2528
#define LP_MODE2_FPS                30

/*Parameter setting for mode 0 Start*/
#define MODE_0_FPS                  60
#define MODE_0_VFP                  54
#define MODE_0_HFP                  484
#define MODE_0_DATA_RATE            834
/*Parameter setting for mode 0 End*/

/*Parameter setting for mode 1 Start*/
#define MODE_1_FPS                  90
#define MODE_1_VFP                  54
#define MODE_1_HFP                  174
#define MODE_1_DATA_RATE            834
/*Parameter setting for mode 1 End*/

/*Parameter setting for mode 2 Start*/
#define MODE_2_FPS                  120
#define MODE_2_VFP                  54
#define MODE_2_HFP                  20
#define MODE_2_DATA_RATE            834
/*Parameter setting for mode 2 End*/

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

static struct regulator *disp_dvdd;

static char bl_tb0[] = {0x51, 0x3, 0xff};
static char *panel_name = "panel_name=dsi_k10a_36_02_0a_dsc_vdo";

#define lcm_dcs_write_seq(ctx, seq...)                                         \
	({                                                                     \
		const u8 d[] = { seq };                                        \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                          \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                                  \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
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


#if 0
static void push_panel_table(struct LCM_setting_table *table, struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle, int len)
{
	unsigned int i = 0;
	pr_info("%s +\n", __func__);

	for (i = 0; i < len; i++) {
		unsigned int cmd;

		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(table[i].count);
			break;
		case REGFLAG_UDELAY:
			udelay(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			cb(dsi, handle, table[i].para_list,
				table[i].count);
		}
	}
	pr_info("%s -\n", __func__);
}
#endif

static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i,j;
	unsigned char temp[30] = {0};
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
				for (j = 0; j < table[i].count; j++ ) {
					temp[j+1] = table[i].para_list[j];
				}
				pr_info("push_table %d: %d", i, table[i].count);
				lcm_dcs_write(ctx, temp, table[i].count+1);
		}
	}
}

static struct LCM_setting_table lcm_suspend_setting[] = {
	{0xFF, 1, {0x10} },
	{0xFB, 1, {0x01} },
	{0x28, 1, {} },
	{REGFLAG_DELAY, 10, {} },
	{0x10, 1, {} },
	{REGFLAG_DELAY, 120, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_init_setting[] = {
	{0xFF, 1,  {0x10} },
	{0xFB, 1,  {0x01} },
	{0x3B, 5,  {0x03,0x14,0x36,0x04,0x04} },
	{0xB0, 1,  {0x00} },
	{0xC2, 2,  {0x1B,0xA0} },

	{0xFF, 1,  {0xE0} },
	{0xFB, 1,  {0x01} },
	{0x35, 1,  {0x82} },

	{0xFF, 1,  {0xF0} },
	{0xFB, 1,  {0x01} },
	{0x1C, 1,  {0x01} },
	{0x33, 1,  {0x01} },
	{0x5A, 1,  {0x00} },
	{0x9C, 1,  {0x00} },

	{0xFF, 1,  {0xD0} },
	{0xFB, 1,  {0x01} },
	{0x53, 1,  {0x22} },
	{0x54, 1,  {0x02} },

	{0xFF, 1,  {0xC0} },
	{0xFB, 1,  {0x01} },
	{0x9C, 1,  {0x11} },
	{0x9D, 1,  {0x11} },

	{0xFF, 1,  {0x25} },
	{0xFB, 1,  {0x01} },
	/* 22:60.5HZ，21:91.2HZ，20:121.1HZ */
	{0x18, 1,  {0x22} },

	{0xFF, 1,  {0x10} },
	{0xFB, 1,  {0x01} },
	/* DSC:03  NO DSC:00 */
	{0xC0, 1,  {0x03} },
	{0xC1, 16,  {0x89,0x28,0x00,0x14,0x00,0xAA,0x02,0x0E,0x00,0x71,0x00,0x07,0x05,0x0E,0x05,0x16} },
	{0xC2, 2,  {0x1B,0xA0} },

	{0x51, 2,  {0x03,0xFF}},

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
	{REGFLAG_DELAY, 30, {} },

	{0xFF, 1, {0x27} },
	{0xFB, 1, {0x01} },
	{0x3F, 1, {0x01} },
	{0x43, 1, {0x08} },
	{0x40, 1, {0x25} },
	{0xFF, 1, {0x10} },
	{0xFB, 1, {0x01} },

	{REGFLAG_END_OF_TABLE, 0x00, {}}

};

static void lcm_panel_init(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(10 * 1000, 15 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, 15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	push_table(ctx, lcm_init_setting, sizeof(lcm_init_setting) / sizeof(struct LCM_setting_table));

	pr_info("%s- k10a-36-02-0a\n", __func__);
}

static struct regulator *disp_vm18;

static int lcm_panel_vm18_regulator_init(struct device *dev)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_vm18 = regulator_get(dev, "vm18");
	if (IS_ERR(disp_vm18)) { /* handle return value */
		ret = PTR_ERR(disp_vm18);
		pr_err("get disp_vci fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */
}

static int lcm_panel_vm18_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_err("%s +\n",__func__);

	lcm_panel_vm18_regulator_init(dev);

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_vm18, 1800000, 1800000);
	if (ret < 0)
		pr_err("set voltage disp_vci fail, ret = %d\n", ret);
	retval |= ret;

	status = regulator_is_enabled(disp_vm18);
	pr_err("%s regulator_is_enabled=%d\n",__func__,status);
	if(!status){
	/* enable regulator */
		ret = regulator_enable(disp_vm18);
		if (ret < 0)
			pr_err("enable regulator disp_vci fail, ret = %d\n", ret);
			retval |= ret;
	}

	pr_err("%s -\n",__func__);
	return retval;
}
static unsigned int start_up = 1;
static int lcm_panel_vm18_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;

	pr_err("%s +\n",__func__);

	lcm_panel_vm18_regulator_init(dev);

	if(start_up) {
		pr_err("%s enable regulator\n",__func__);
		/* enable regulator */
		ret = regulator_enable(disp_vm18);
		if (ret < 0)
			pr_err("enable regulator disp_vci fail, ret = %d\n", ret);
			retval |= ret;

			start_up = 0;
	}

	ret = regulator_disable(disp_vm18);
	if (ret < 0)
		pr_err("disable regulator disp_vci fail, ret = %d\n", ret);
	retval |= ret;

	pr_err("%s -\n",__func__);

	return retval;
}

static int lcm_panel_dvdd_regulator_init(struct device *dev)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_dvdd = regulator_get(dev, "dvdd");
	if (IS_ERR(disp_dvdd)) { /* handle return value */
		ret = PTR_ERR(disp_dvdd);
		pr_err("get disp_dvdd fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */
}

static int lcm_panel_dvdd_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_err("%s +\n",__func__);

	lcm_panel_dvdd_regulator_init(dev);

	status = regulator_is_enabled(disp_dvdd);
	pr_err("%s regulator_is_enabled=%d\n",__func__,status);
	if(!status){
	/* enable regulator */
		ret = regulator_enable(disp_dvdd);
		if (ret < 0)
			pr_err("enable regulator disp_dvdd fail, ret = %d\n", ret);
			retval |= ret;
	}

	pr_err("%s -\n",__func__);
	return retval;
}

static unsigned int dvdd_start_up = 1;
static int lcm_panel_dvdd_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;

	pr_err("%s +\n",__func__);

	lcm_panel_dvdd_regulator_init(dev);

	if(dvdd_start_up) {
		pr_err("%s enable regulator\n",__func__);
		/* enable regulator */
		ret = regulator_enable(disp_dvdd);
		if (ret < 0)
			pr_err("enable regulator disp_dvdd fail, ret = %d\n", ret);
			retval |= ret;

			dvdd_start_up = 0;
	}

	ret = regulator_disable(disp_dvdd);
	if (ret < 0)
		pr_err("disable regulator disp_vci fail, ret = %d\n", ret);
	retval |= ret;

	pr_err("%s -\n",__func__);

	return retval;
}

static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx;
	ctx = panel_to_lcm(panel);
	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}
/*AVDD			-->	VSP  	  --> LM36273_VREG_DISP_P(GPIO126/bias0)*/
/*AVEE			-->	VSN		  --> LM36273_VREG_DISP_N(GPIO126/bias0)*/
/*DVDD			-->	LCD_DVDD  --> WL2866_DVDD2(GPIO148/bias1)*/
/*LCD_VDDIO_1P8	-->	LCD_1P8   --> PMIC_VM18*/
static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	if (!ctx->prepared)
		return 0;

	push_table(ctx, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table));

	if (is_tp_doubleclick_enable() == false || get_panel_dead_flag()) {
		ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->reset_gpio, 0);
		devm_gpiod_put(ctx->dev, ctx->reset_gpio);

		/* AVDD&AVEE Power off */
		ctx->bias_pos =
		    devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
		lm36273_brightness_set(0);
		lm36273_bias_enable(0, 3);

		/* DVDD Power off */
		lcm_panel_dvdd_disable(ctx->dev);

		usleep_range(4 * 1000, 5 * 1000);
		/* VDDI Power off */
		lcm_panel_vm18_disable(ctx->dev);

	} else {
		/* DVDD Power off */
		pr_info("%s is_tp_doubleclick_enable = true\n", __func__);
		lcm_panel_dvdd_disable(ctx->dev);
	}

	ctx->error = 0;
	ctx->prepared = false;
	ctx->hbm_en = false;
	panel->panel_initialized = false;

	/* add for display fps cpunt */
	dsi_panel_fps_count(ctx, 0, 0);
	/* add for display state count*/
	if (ctx->mi_count.panel_active_count_enable)
		dsi_panel_state_count(ctx, 0);
	/* add for display hbm count */
	dsi_panel_HBM_count(ctx, 0, 1);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s+\n", __func__);
	if (ctx->prepared)
		return 0;

	if (is_tp_doubleclick_enable() == false || get_panel_dead_flag()) {
		/* VDDI Power on */
		lcm_panel_vm18_enable(ctx->dev);
		udelay(1 * 1000);

		/* DVDD Power on */
		/* enable regulator */
		lcm_panel_dvdd_enable(ctx->dev);
		udelay(1 * 1000);

		/* AVDD&AVEE Power on*/
		ctx->bias_pos =
			devm_gpiod_get_index(ctx->dev, "bias", 0, GPIOD_OUT_HIGH);
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
		lm36273_bl_bias_conf();
		lm36273_bias_enable(1, 1);
		usleep_range(15 * 1000, 20 * 1000);
	} else {
		/* DVDD Power on */
		/* enable regulator */
		lcm_panel_dvdd_enable(ctx->dev);
	}

	//lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;
	ctx->hbm_en = false;
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

static const struct drm_display_mode lowpower_mode0 = {
	.clock = 316325,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,			//HFP
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,		//HSA
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,		//HBP
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + LP_MODE0_VFP,			//VFP
	.vsync_end = FRAME_HEIGHT + LP_MODE0_VFP + VSA,		//VSA
	.vtotal = FRAME_HEIGHT + LP_MODE0_VFP + VSA + VBP,	//VBP
	.vrefresh = LP_MODE0_FPS,
};

static const struct drm_display_mode lowpower_mode1 = {
	.clock = 316325,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,			//HFP
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,		//HSA
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,		//HBP
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + LP_MODE1_VFP,			//VFP
	.vsync_end = FRAME_HEIGHT + LP_MODE1_VFP + VSA,		//VSA
	.vtotal = FRAME_HEIGHT + LP_MODE1_VFP + VSA + VBP,	//VBP
	.vrefresh = LP_MODE1_FPS,
};

static const struct drm_display_mode lowpower_mode2 = {
	.clock = 316325,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,			//HFP
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,		//HSA
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,		//HBP
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + LP_MODE2_VFP,			//VFP
	.vsync_end = FRAME_HEIGHT + LP_MODE2_VFP + VSA,		//VSA
	.vtotal = FRAME_HEIGHT + LP_MODE2_VFP + VSA + VBP,	//VBP
	.vrefresh = LP_MODE2_FPS,
};

static const struct drm_display_mode default_mode = {
	.clock = 316325,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,			//HFP
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,		//HSA
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,		//HBP
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,			//VFP
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,		//VSA
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,	//VBP
	.vrefresh = MODE_0_FPS,
};

static const struct drm_display_mode performance_mode = {
	.clock = 369170,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,			//HFP
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,		//HSA
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,		//HBP
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,			//VFP
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,		//VSA
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,	//VBP
	.vrefresh = MODE_1_FPS,
};

static const struct drm_display_mode performance_mode1 = {
	.clock = 422163,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,			//HFP
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,		//HSA
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,		//HBP
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,			//VFP
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,		//VSA
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,	//VBP
	.vrefresh = MODE_2_FPS,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_30hz = {
	.pll_clk = 417,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 68785,
	.physical_height_um = 152856,
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
	.data_rate = DATA_RATE ,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
	.vfp_low_power = LP_MODE2_VFP,//30hz
	.lfr_enable = 0,
	.lfr_minimum_fps = 30,
	.dyn_fps = {
		.switch_en = 0,
		.vact_timing_fps = 30,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 0,
		.pll_clk = 417,
		.vfp_lp_dyn = LP_MODE2_VFP,
		.hfp = MODE_0_HFP,
		.vfp = LP_MODE2_VFP,
	},
};

static struct mtk_panel_params ext_params_48hz = {
	.pll_clk = 417,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 68785,
	.physical_height_um = 152856,
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
	.data_rate = DATA_RATE ,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
	.vfp_low_power = LP_MODE1_VFP,//48hz
	.lfr_enable = 0,
	.lfr_minimum_fps = 48,
	.dyn_fps = {
		.switch_en = 0,
		.vact_timing_fps = 48,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 0,
		.pll_clk = 417,
		.vfp_lp_dyn = LP_MODE1_VFP,
		.hfp = MODE_0_HFP,
		.vfp = LP_MODE1_VFP,
	},
};

static struct mtk_panel_params ext_params_50hz = {
	.pll_clk = 417,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 68785,
	.physical_height_um = 152856,
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
	.data_rate = DATA_RATE ,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
	.vfp_low_power = LP_MODE0_VFP,//50hz
	.lfr_enable = 0,
	.lfr_minimum_fps = 50,
	.dyn_fps = {
		.switch_en = 0,
		.vact_timing_fps = 50,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 0,
		.pll_clk = 417,
		.vfp_lp_dyn = LP_MODE0_VFP,
		.hfp = MODE_0_HFP,
		.vfp = LP_MODE0_VFP,
	},
};

static struct mtk_panel_params ext_params = {
	.pll_clk = 417,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 68785,
	.physical_height_um = 152856,
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
	.data_rate = DATA_RATE ,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
	.vfp_low_power = 879,//45hz
	.lfr_enable = 0,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 1,
#if HFP_SUPPORT
		.vact_timing_fps = 60,
#else
		.vact_timing_fps = 120,
#endif
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x22} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 0,
		.pll_clk = 417,
		.vfp_lp_dyn = 4178,
		.hfp = MODE_0_HFP,
		.vfp = 54,
	},
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 417,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 68785,
	.physical_height_um = 152856,
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
	.data_rate = DATA_RATE ,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
	.vfp_low_power = 1294,//60hz
	.lfr_enable = 0,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 1,
#if HFP_SUPPORT
		.vact_timing_fps = 90,
#else
		.vact_timing_fps = 120,
#endif
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x21} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 0,
		.pll_clk = 417,
		.vfp_lp_dyn = 2528,
		.hfp = MODE_1_HFP,
		.vfp = 54,
	},
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 417,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0A, .count = 1, .para_list[0] = 0x9C,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 68785,
	.physical_height_um = 152856,
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
	.data_rate = DATA_RATE,
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en=1,
	.corner_pattern_height=ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot=ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
	.vfp_low_power = 2528,//idle 60hz
	.lfr_enable = 0,
	.lfr_minimum_fps = 60,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.dfps_cmd_table[0] = {0, 2, {0xFF, 0x25} },
		.dfps_cmd_table[1] = {0, 2, {0xFB, 0x01} },
		.dfps_cmd_table[2] = {0, 2, {0x18, 0x20} },
	},
	/* following MIPI hopping parameter might cause screen mess */
	.dyn = {
		.switch_en = 0,
		.pll_clk = 417,
		.vfp_lp_dyn = 2528,
		.hfp = MODE_2_HFP,
		.vfp = 54,
	},
};

static int panel_ata_check(struct drm_panel *panel)
{
	/* Customer test by own ATA tool */
	return 1;
}

static int lcm_setbacklight_cmdq(void *dsi,
		dcs_write_gce cb, void *handle, unsigned int level)
{
	bl_tb0[1] = (level >> 8) & 0x7;
	bl_tb0[2] = level & 0xFF;
	pr_info("%s+ \n", __func__);
	pr_info("%s level = %d\n",  __func__, level);

	if (!cb)
		return -1;

	//cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	pr_info("%s-\n", __func__);
	return 0;
}

static int lcm_setbacklight_i2c(struct drm_panel *panel, unsigned int level)
{
	struct lcm *ctx = panel_to_lcm(panel);

	bl_tb0[1] = (level >> 8) & 0x7;
	bl_tb0[2] = level & 0xFF;
	lm36273_brightness_set(level);

	/* add for display backlight count */
	dsi_panel_backlight_count(ctx, level);

	return 0;
}

static struct drm_display_mode *get_mode_by_id_hfp(struct drm_panel *panel,
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

static void lcm_esd_restore_backlight(struct drm_panel *panel)
{
	unsigned int level;
	level = (bl_tb0[1] << 8) + bl_tb0[2];
	lcm_setbacklight_i2c(panel, level);
	pr_info("%s high_bl = 0x%x, low_bl = 0x%x \n", __func__, bl_tb0[1], bl_tb0[2]);
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

	if (op == LM36273_REG_READ) {
		for (i = 0; i < count; i++) {
			ret = lm36273_reg_read_bytes(reg_addr, reg_val);
			if (ret <= 0)
				break;

			reg_addr++;
			reg_val++;
		}
	} else if (op == LM36273_REG_WRITE) {
		ret = lm36273_reg_write_bytes(reg_addr, *(reg_val + 1));
	}

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(panel, mode);

	if (m->vrefresh == MODE_0_FPS) {
		ext->params = &ext_params;
#if HFP_SUPPORT
		current_fps = 60;
#endif
	} else if (m->vrefresh == MODE_1_FPS) {
		ext->params = &ext_params_90hz;
#if HFP_SUPPORT
		current_fps = 90;
#endif
	} else if (m->vrefresh == MODE_2_FPS) {
		ext->params = &ext_params_120hz;
#if HFP_SUPPORT
		current_fps = 120;
#endif
	} else if (m->vrefresh == LP_MODE0_FPS) {
		ext->params = &ext_params_50hz;
#if HFP_SUPPORT
		current_fps = 50;
#endif
	} else if (m->vrefresh == LP_MODE1_FPS) {
		ext->params = &ext_params_48hz;
#if HFP_SUPPORT
		current_fps = 48;
#endif
	} else if (m->vrefresh == LP_MODE2_FPS) {
		ext->params = &ext_params_30hz;
#if HFP_SUPPORT
		current_fps = 30;
#endif
	} else
		ret = 1;

	return ret;
}

static void mode_switch_to_120(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x20);
}

static void mode_switch_to_90(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x21);
}

static void mode_switch_to_60(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x25);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x22);
}

static int mode_switch(struct drm_panel *panel, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id_hfp(panel, dst_mode);

	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	if (m->vrefresh == 60) { /* 60 switch to 120 */
		mode_switch_to_60(panel);
	} else if (m->vrefresh == 90) { /* 1200 switch to 60 */
		mode_switch_to_90(panel);
	} else if (m->vrefresh == 120) { /* 1200 switch to 60 */
		mode_switch_to_120(panel);
	} else
		ret = 1;

	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("enter panel_ext_reset\n");
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, on);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	return 0;
}

static int panel_normal_hbm_control(struct drm_panel *panel, uint32_t level)
{
	struct lcm *ctx = panel_to_lcm(panel);
	uint32_t lcd_hbm_level = level;
	bool en = false;

	/* Switch old disp params to new displayfeature value*/
	switch (level) {
		case DISPPARAM_LCD_HBM_L1_ON:
			lcd_hbm_level = LCD_HBM_L1_ON;
			en = false;
			break;
		case DISPPARAM_LCD_HBM_L2_ON:
			lcd_hbm_level = LCD_HBM_L2_ON;
			en = true;
			break;
		case DISPPARAM_LCD_HBM_L3_ON:
			lcd_hbm_level = LCD_HBM_L3_ON;
			en = true;
			break;
		case DISPPARAM_LCD_HBM_OFF:
			lcd_hbm_level = LCD_HBM_OFF;
			en = false;
			break;
		default:
			break;

	/* add for display hbm count */
	if (en)
		dsi_panel_HBM_count(ctx, 1, 0);
	else
		dsi_panel_HBM_count(ctx, 0, 0);
	}

	return hbm_brightness_set(lcd_hbm_level);
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.init = lcm_panel_init,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ext_param_set = mtk_panel_ext_param_set,
	.mode_switch = mode_switch,
	.esd_restore_backlight = lcm_esd_restore_backlight,
	.get_panel_info = panel_get_panel_info,
	.ata_check = panel_ata_check,
	.set_backlight_i2c = lcm_setbacklight_i2c,
	.led_i2c_reg_op = lcm_led_i2c_reg_op,
	.normal_hbm_control = panel_normal_hbm_control,
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
	 *	   become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *	  display the first valid frame after starting to receive
	 *	  video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *	   turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *		 to power itself down completely
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
	struct drm_display_mode *mode1;
	struct drm_display_mode *mode2;
	struct drm_display_mode *mode3;
	struct drm_display_mode *mode4;
	struct drm_display_mode *mode5;
	struct drm_display_mode *mode6;

	/* 60hz */
	mode1 = drm_mode_duplicate(panel->drm, &default_mode);
	if (!mode1) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 default_mode.hdisplay, default_mode.vdisplay,
			 default_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode1);

	/* 90hz */
	mode2 = drm_mode_duplicate(panel->drm, &performance_mode);
	if (!mode2) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode.hdisplay, performance_mode.vdisplay,
			 performance_mode.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode2);

	/* 120hz */
	mode3 = drm_mode_duplicate(panel->drm, &performance_mode1);
	if (!mode3) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 performance_mode1.hdisplay, performance_mode1.vdisplay,
			 performance_mode1.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(panel->connector, mode3);

	/* 50hz */
	mode4 = drm_mode_duplicate(panel->drm, &lowpower_mode0);
	if (!mode4) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 lowpower_mode0.hdisplay, lowpower_mode0.vdisplay,
			 lowpower_mode0.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode4);
	mode4->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode4);

	/* 48hz */
	mode5 = drm_mode_duplicate(panel->drm, &lowpower_mode1);
	if (!mode5) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 lowpower_mode1.hdisplay, lowpower_mode1.vdisplay,
			 lowpower_mode1.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode5);
	mode5->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode5);

	/* 30hz */
	mode6 = drm_mode_duplicate(panel->drm, &lowpower_mode2);
	if (!mode6) {
		dev_info(panel->drm->dev, "failed to add mode %ux%ux@%u\n",
			 lowpower_mode2.hdisplay, lowpower_mode2.vdisplay,
			 lowpower_mode2.vrefresh);
		return -ENOMEM;
	}

	drm_mode_set_name(mode6);
	mode6->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(panel->connector, mode6);

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

	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET;

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
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->bias_pos)) {
		dev_info(dev, "cannot get bias-gpios 0 %ld\n",
			 PTR_ERR(ctx->bias_pos));
		return PTR_ERR(ctx->bias_pos);
	}
	devm_gpiod_put(dev, ctx->bias_pos);

	ctx->prepared = true;
	ctx->enabled = true;
	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;
	ctx->panel_info = panel_name;
	ctx->panel.panel_initialized = true;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0)
		return ret;


	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

#endif

	dsi_panel_count_init(ctx);
	pr_info("%s- k10a 36 02 0a dsc vdo panel\n", __func__);

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
		.compatible = "k10a_36_02_0a_dsc_vdo,lcm",
	},
	{} };

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {

			.name = "k10a_36_02_0a_dsc_vdo,lcm",
			.owner = THIS_MODULE,
			.of_match_table = lcm_of_match,
		},
};

module_mipi_dsi_driver(lcm_driver);
