// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include "../mediatek/mediatek_v2/mtk_drm_arr.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "mtk_panel_ext.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_log.h"
#endif
#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "mi_dsi_panel.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#include "include/panel_n16u_42_02_0a_alpha_data.h"

#define REGFLAG_CMD				0xFFFA
#define REGFLAG_DELAY			0xFFFC
#define REGFLAG_UDELAY			0xFFFB
#define REGFLAG_END_OF_TABLE    	0xFFFD
#define REGFLAG_RESET_LOW		0xFFFE
#define REGFLAG_RESET_HIGH		0xFFFF
#define DATA_RATE                   1100

#define FRAME_WIDTH                 (1220)
#define FRAME_HEIGHT                (2712)

#define DSC_ENABLE                  1
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 40
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         10
#define DSC_DSC_LINE_BUF_DEPTH      11
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
#define DSC_SLICE_HEIGHT            12
#define DSC_SLICE_WIDTH             610
#define DSC_CHUNK_SIZE              610
#define DSC_XMIT_DELAY              512
#define DSC_DEC_DELAY               562
#define DSC_SCALE_VALUE             32
#define DSC_INCREMENT_INTERVAL      305
#define DSC_DECREMENT_INTERVAL      8
#define DSC_LINE_BPG_OFFSET         12
#define DSC_NFL_BPG_OFFSET          2235
#define DSC_SLICE_BPG_OFFSET        1915
#define DSC_INITIAL_OFFSET          6144
#define DSC_FINAL_OFFSET            4336
#define DSC_FLATNESS_MINQP          7
#define DSC_FLATNESS_MAXQP          16
#define DSC_RC_MODEL_SIZE           8192
#define DSC_RC_EDGE_FACTOR          6
#define DSC_RC_QUANT_INCR_LIMIT0    15
#define DSC_RC_QUANT_INCR_LIMIT1    15
#define DSC_RC_TGT_OFFSET_HI        3
#define DSC_RC_TGT_OFFSET_LO        3

#define MAX_BRIGHTNESS_CLONE        16383
#define FACTORY_MAX_BRIGHTNESS      2047

#define PHYSICAL_WIDTH              69540
#define PHYSICAL_HEIGHT             154584

#define FPS_INIT_INDEX 43
#define GIR_INIT_INDEX1 44

#define TEMP_INDEX1_36 0x01
#define TEMP_INDEX2_32 0x20
#define TEMP_INDEX3_28 0x40
#define TEMP_INDEX4_off 0x80
#define TEMP_INDEX 42

#define LCM_P0_0 0x00
#define LCM_P0_1 0x10
#define LCM_P1_0 0x40
#define LCM_P1_1 0x50
#define LCM_P2_0 0x80

static u8 build_id = 0;
static atomic_t doze_enable = ATOMIC_INIT(0);
static int doze_hbm_dbv_level = 242;
static int doze_lbm_dbv_level = 20;
static int normal_max_bl = 2047;
static bool dynamic_change_fps_going = false;
static const int panel_id= PANEL_1ST;
static struct lcm *panel_ctx;
static struct drm_panel * this_panel = NULL;
static struct mtk_dsi *globle_mtk_dsi = NULL;
static unsigned int last_non_zero_bl_level = 511;
static char oled_wp_cmdline[18] = {0};
static char oled_lhbm_cmdline[80] = {0};
static bool lhbm_w1200_update_flag = true;
static bool lhbm_w250_update_flag = true;
static bool lhbm_g500_update_flag = true;
static bool lhbm_w1200_readbackdone;
static bool lhbm_w250_readbackdone;
static bool lhbm_g500_readbackdone;
struct LHBM_WHITEBUF {
	unsigned char lhbm_1200[6];
	unsigned char lhbm_250[6];
	unsigned char lhbm_500[6];
};
static struct LHBM_WHITEBUF lhbm_whitebuf;
enum lhbm_cmd_type {
	TYPE_WHITE_1200 = 0,
	TYPE_WHITE_250,
	TYPE_GREEN_500,
	TYPE_LHBM_OFF,
	TYPE_HLPM_W1200,
	TYPE_HLPM_W250,
	TYPE_HLPM_G500,
	TYPE_HLPM_OFF,
	TYPE_MAX
};

static const char *panel_name = "panel_name=dsi_n16u_42_02_0a_dsc_vdo";
struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode);

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

static struct regulator *disp_vci;
static int lcm_panel_vci_regulator_init(struct device *dev)
{
	static int vibr_regulator_inited;
	int ret = 0;

	if (vibr_regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_vci = regulator_get(dev, "vibr30");
	if (IS_ERR(disp_vci)) { /* handle return value */
		ret = PTR_ERR(disp_vci);
		pr_err("get disp_vci fail, error: %d\n", ret);
		return ret;
	}

	vibr_regulator_inited = 1;
	return ret; /* must be 0 */
}

static unsigned int vibr_start_up = 1;
static int lcm_panel_vci_enable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_info("%s +\n",__func__);

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_vci, 3000000, 3000000);
	if (ret < 0)
		pr_err("set voltage disp_vci fail, ret = %d\n", ret);
	retval |= ret;

	status = regulator_is_enabled(disp_vci);
	pr_info("%s regulator_is_enabled = %d, vibr_start_up = %d\n", __func__, status, vibr_start_up);
	if(!status || vibr_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_vci);
		if (ret < 0)
			pr_err("enable regulator disp_vci fail, ret = %d\n", ret);
		vibr_start_up = 0;
		retval |= ret;
	}

	pr_info("%s -\n",__func__);
	return retval;
}

static int lcm_panel_vci_disable(struct device *dev)
{
	int ret = 0;
	int retval = 0;
	int status = 0;

	pr_info("%s +\n",__func__);

	status = regulator_is_enabled(disp_vci);
	pr_info("%s regulator_is_enabled = %d\n", __func__, status);
	if(status){
		ret = regulator_disable(disp_vci);
		if (ret < 0)
			pr_err("disable regulator disp_vci fail, ret = %d\n", ret);
	}
	retval |= ret;

	pr_info("%s -\n",__func__);

	return retval;
}

static struct LCM_setting_table init_setting_vdo[] = {
	/*cmd2 page 8*/
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x00} },
	{0x6F, 1,  {0x03} },
	{0xC0, 1,  {0x21} },
	/*cmd2 page 1*/
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x01} },
	/*AOD VGSP&VGMP*/
	{0x6F, 1,  {0x05} },
	{0xE3, 2,  {0x00,0x00} },
	{0x6F, 1,  {0x0A} },
	{0xE3, 4,  {0x00,0x00,0x00,0x00} },
	/*cmd2 page 8*/
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x08} },
	{0x6F, 1,  {0xCE} },
	/*ic dimming on*/
	{0xBC, 1,  {0x01} },
	{0x6F, 1,  {0xCF} },
	/*ic dimming time*/
	{0xBC, 8,  {0x01,0x11,0x00,0xB6,0x01,0x11,0x00,0x44} },
	/*VDC gamma conversion enable*/
	{0xBE, 1,  {0x03} },
	{0x6F, 1,  {0x0C} },
	/*VDC preset*/
	{0xE9, 8,  {0x1D,0x36,0x0F,0xD6,0x1C,0x51,0x0F,0xAB} },
	/*cmd3 page 0*/
	{0xFF, 4,  {0xAA,0x55,0xA5,0x80} },
	/*SD Optimize*/
	{0x6F, 1,  {0x2A} },
	{0xF4, 1,  {0x08} },
	{0x6F, 1,  {0x46} },
	{0xF4, 2,  {0x07,0x09} },
	{0x6F, 1,  {0x4A} },
	{0xF4, 2,  {0x08,0x0A} },
	{0x6F, 1,  {0x56} },
	{0xF4, 2,  {0x44,0x44} },
	/*cmd3 page 1*/
	{0xFF, 4,  {0xAA,0x55,0xA5,0x81} },
	/*hs_drop_detect_en = 1, hs_drop_detect_period[2:0] = 4*/
	{0x6F, 1,  {0x3C} },
	{0xF5, 1,  {0x84} },
	{0x17, 1,  {0x03} },
	/*Video mode AOD setting*/
	{0x71, 1,  {0x00} },
	{0x8D, 8,  {0x00,0x00,0x04,0xC3,0x00,0x00,0x0A,0x97} },
	/*colum*/
	{0x2A, 4,  {0x00,0x00,0x04,0xC3} },
	/*Row*/
	{0x2B, 4,  {0x00,0x00,0x0A,0x97} },
	/*Vesa Decode emable*/
	{0x90, 2,  {0x03,0x43} },
	/*Dsc setting*/
	{0x91,18,  {0xAB,0x28,0x00,0x0C,0xC2,0x00,0x02,0x32,0x01,0x31,0x00,0x08,0x08,0xBB,0x07,0x7B,0x10,0xF0} },
	/*VBP&VFP*/
	{0x3B, 12,  {0x00,0x0B,0x00,0x2D,0x00,0x0B,0x03,0xCD,0x00,0x0B,0x0A,0xFD} },
	{0x6F, 1,  {0x10} },
	/*IDLE VBP & VFP*/
	{0x3B, 4,  {0x00,0x0B,0x00,0x2D} },
	/*TE on*/
	{0x35, 1,  {0x00} },
	/*Aod mode*/
	{0x6F, 1,  {0x04} },
	{0x51, 2,  {0x00,0x00} },
	/*Brightness Control*/
	{0x53, 1, {0x20}},
	/*3D LUT OFF*/
	{0x57, 1,  {0x00} },
	/*60hz*/
	{0x2F, 1,  {0x02} },
	/*Gir off*/
	{0x5F, 2,  {0x04,0x40} },

	/*Improve elvss current backfeeding*/
	{0xFF, 4,  {0xAA,0x55,0xA5,0x80} },
	{0x6F, 1,  {0x31} },
	{0xFC, 1,  {0x30} },
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x01} },
	{0x6F, 1,  {0x0A} },
	{0xE4, 1,  {0x90} },

	/*spr*/
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x07} },
	{0xB0, 6,  {0x8C,0xC0,0x78,0x70,0x00,0x00} },
	{0xB1, 6,  {0x1C,0x0C,0x00,0x0C,0x1C,0x00} },
	{0xB2, 9,  {0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28} },
	{0x6F, 1,  {0x36} },
	{0xB2, 18,  {0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C} },
	{0x6F, 1,  {0x09} },
	{0xB2, 9,  {0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28} },
	{0x6F, 1,  {0x48} },
	{0xB2, 18,  {0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C} },
	{0x6F, 1,  {0x1B} },
	{0xB2, 9,  {0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28} },
	{0x6F, 1,  {0x6C} },
	{0xB2, 18,  {0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C} },
	{0x6F, 1,  {0x24} },
	{0xB2, 9,  {0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28} },
	{0x6F, 1,  {0x7E} },
	{0xB2, 18,  {0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x32,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C} },
	{0xB3, 1,  {0x6C} },

	/*open esd detect*/
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x00} },
	{0xBE, 2,  {0x47,0x45} },
	/*normal high,active low*/
	{0x6F, 1,  {0x05} },
	{0xBE, 1,  {0xA8} },
	{0x6F, 1,  {0x19} },
	{0xBE, 4,  {0x10,0x91,0x00,0xAB} },
	{0x6F, 1,  {0x0D} },
	{0xD8, 1,  {0x02} },

	/*cmd2 page 1*/
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x01} },
	/*lvdt on*/
	{0x6F, 1,  {0x03} },
	{0xC7, 1,  {0xC7} },

	/*init LHBM*/
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x00} },
	/*FPR_MODE = 0, FPR_ELVSS_EN = 1, FPR_ELVSS_DELAY = 1*/
	{0xDF, 1,  {0x09} },
	/*FPR_EM_SWITCH_EN = 1, FPR_ALPHA_LUT_EN = 1*/
	{0x6F, 1,  {0x01} },
	{0xDF, 1,  {0x40} },
	/*FPR_GMA_SEL[3:0] = 0, FPR_DGMA_SET_EN = 1, FPR_DGMA_SET_SEL[3:0] = 11*/
	{0x6F, 1,  {0x31} },
	{0xDF, 2,  {0x00,0x1A} },
	/*FPR_ELVSS[7:0] = 47 -2v(NT50380)  =35  -2v(SGM3836A)*/
	{0x6F, 1,  {0x34} },
	{0xDF, 1,  {0x2F} },
	/*cmd2 page 1*/
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x01} },
	/*FPR_VINITP1_EN = 0, FPR_VINITN1_EN = 1, FPR_VINITN2_EN = 0, FPR_VINITN3_EN = 0*/
	{0x6F, 1,  {0x2A} },
	{0xB9, 1,  {0x10} },
	/*FPR_VINITN1 = (0x1A=-1.3V)*/
	{0x6F, 1,  {0x0C} },
	{0xB9, 1,  {0x1A} },
	/*FPR1_CENTER_X[11:0] = 609, FPR1_CENTER_Y[12:0] = 2437*/
	{0x88, 5,  {0x81,0x02,0x61,0x09,0x85} },
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x00} },
	{0x6F, 1,  {0x2D} },
	/*FR1_Lut_SEL[3:0] 3:D4(90HZ);  FR1 2:D5(60Hz)*/
	{0xC0, 5,  {0x00,0x32,0x00,0x04,0x50} },
	{0x6F, 1,  {0x01} },
	{0xC7, 1,  {0x01} },

	/*gir code*/
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x08} },
	{0xBE, 16,  {0x03,0xF1,0x00,0x64,0xF8,0xC0,0x01,0x3A,0x99,0xDF,0x01,0x3A,0x99,0xDF,0x01,0x16} },
	{0x6F, 1, {0x10}},
	{0xBE, 16,  {0x5C,0xFC,0x01,0x16,0x5C,0xFC,0x01,0x3A,0x99,0xDF,0x01,0x3A,0x99,0xDF,0x01,0x16} },
	{0x6F, 1, {0x20}},
	{0xBE, 16,  {0x5C,0xFC,0x01,0x16,0x5C,0xFC,0x01,0x8A,0xC5,0x09,0x01,0x8A,0xC5,0x09,0x80,0x34} },
	{0x6F, 1, {0x30}},
	{0xBE, 3, {0x00,0x00,0x00}},

	/*video trim*/
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x01} },
	{0xEA, 1,  {0x91} },
	{0x6F, 1,  {0x08} },
	{0xEA, 1,  {0x48} },
	{0x6F, 1,  {0x0B} },
	{0xEA, 1,  {0x91} },
	{0x6F, 1,  {0x13} },
	{0xEA, 1,  {0x72} },
	{0x6F, 1,  {0x04} },
	{0xC3, 1,  {0x0A} },
	{0x6F, 1,  {0x09} },
	{0xC3, 1,  {0x0A} },

	/* abnormal power off */
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x01} },
	{0x6F, 1,  {0x09} },
	{0xC7, 1,  {0x24} },
	{0xF0, 5,  {0x55,0xAA,0x52,0x08,0x05} },
	{0xCB, 7,  {0x33,0x33,0x33,0x33,0x33,0x33,0x33} },

	{0x11, 0,  {} },
	{REGFLAG_DELAY, 85, {} },

	/*gain*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x04} },
	{0xCB, 1, {0x66}},
	/*dbv0 8h*/
	{0xDD, 5, {0x05,0x08,0x1F,0x3E,0x9B} },
	{0x6F, 1, {0x05}},
	/*dbv1 62h*/
	{0xDD, 5, {0x05,0x08,0x1F,0x3E,0x9B} },
	{0x6F, 1, {0x0A}},
	/*dbv2 147h*/
	{0xDD, 5, {0x05,0x08,0x1F,0x3E,0x9B} },
	{0x6F, 1, {0x0F}},
	/*dbv3 4A3h*/
	{0xDD, 5, {0x05,0x08,0x1F,0x3E,0x9B} },
	{0x6F, 1, {0x14}},
	/*dbv4 7FFh*/
	{0xDD, 5, {0x05,0x08,0x1F,0x3E,0x9B} },
	{0x6F, 1, {0x19}},
	/*dbv0 8h*/
	{0xDD, 5, {0x05,0x08,0x1F,0x3E,0x9B} },
	/*LUT1 120HZ*/
	{0x6F, 1, {0x5A}},
	{0xD2, 6, {0x10,0x10,0x10,0x10,0x10,0x14} },
	{0x6F, 1, {0x60}},
	{0xD2, 6, {0x10,0x10,0x10,0x10,0x10,0x0C} },
	{0x6F, 1, {0x66}},
	{0xD2, 6, {0x12,0x12,0x12,0x12,0x12,0x0C} },
	{0x6F, 1, {0x6C}},
	{0xD2, 6, {0x12,0x12,0x12,0x12,0x12,0x0A} },
	{0x6F, 1, {0x72}},
	{0xD2, 6, {0x12,0x12,0x12,0x12,0x12,0x08} },

	{0x6F, 1, {0x78}},
	{0xD2, 6, {0x28,0x28,0x28,0x28,0x28,0x14} },
	{0x6F, 1, {0x7E}},
	{0xD2, 6, {0x1A,0x1A,0x1A,0x1A,0x1A,0x10} },
	{0x6F, 1, {0x84}},
	{0xD2, 6, {0x1A,0x1A,0x1A,0x1A,0x1A,0x10} },
	{0x6F, 1, {0x8A}},
	{0xD2, 6, {0x1A,0x1A,0x1A,0x1A,0x1A,0x10} },
	{0x6F, 1, {0x90}},
	{0xD2, 6, {0x1A,0x1A,0x1A,0x1A,0x1A,0x08} },

	{0x6F, 1, {0x96}},
	{0xD2, 6, {0x14,0x14,0x14,0x14,0x14,0x10} },
	{0x6F, 1, {0x9C}},
	{0xD2, 6, {0x14,0x14,0x14,0x14,0x14,0x08} },
	{0x6F, 1, {0xA2}},
	{0xD2, 6, {0x0D,0x0D,0x0D,0x0D,0x0D,0x08} },
	{0x6F, 1, {0xA8}},
	{0xD2, 6, {0x0D,0x0D,0x0D,0x0D,0x0D,0x08} },
	{0x6F, 1, {0xAE}},
	{0xD2, 6, {0x0B,0x0B,0x0B,0x0B,0x0B,0x08} },

	{0x29, 0, {} },

	/*3d lut*/
	{0xF0, 5, {0x55,0xAA,0x52,0x08,0x00} },
	{0xE6, 1, {0x80}},
	{0x6F, 1, {0xEA}},
	{0xE8, 6, {0x10,0x10,0x20,0x00,0x00,0x00} },
	{0xE6, 1, {0x81}},
	{0x6F, 1, {0xEA}},
	{0xE8, 6, {0x10,0x10,0x20,0x00,0x00,0x00} },
	{0xE6, 1, {0x82}},
	{0x6F, 1, {0xEA}},
	{0xE8, 6, {0x10,0x10,0x20,0x00,0x00,0x00} },
	{0xE6, 1, {0x83}},
	{0x6F, 1, {0xEA}},
	{0xE8, 6, {0x10,0x10,0x20,0x00,0x00,0x00} },
	{0xE6, 1, {0x84}},
	{0x6F, 1, {0xEA}},
	{0xE8, 6, {0x10,0x10,0x20,0x00,0x00,0x00} },
	{0xE6, 1, {0x85}},
	{0x6F, 1, {0xEA}},
	{0xE8, 6, {0x10,0x10,0x20,0x00,0x00,0x00} },
	{0xE6, 1, {0x86}},
	{0x6F, 1, {0xEA}},
	{0xE8, 6, {0x10,0x10,0x20,0x00,0x00,0x00} },
	{0xE6, 1, {0x87}},
	{0x6F, 1, {0xEA}},
	{0xE8, 6, {0x10,0x10,0x20,0x00,0x00,0x00} },
	{0xE6, 1, {0x88}},
	{0x6F, 1, {0xEA}},
	{0xE8, 6, {0x10,0x10,0x20,0x00,0x00,0x00} },
	{0xE6, 1, {0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {}},
};


static struct LCM_setting_table lcm_suspend_setting[] = {
	{0x51, 06, {0x00,0x00,0x00,0x00,0x00,0x00}},
	{0x28, 0, {} },
	{REGFLAG_DELAY, 10, {} },
	{0x10, 0, {} },
	{REGFLAG_DELAY, 17, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};

static struct LCM_setting_table lcm_suspend_setting2[] = {
	{REGFLAG_DELAY, 100, {} },
	{REGFLAG_END_OF_TABLE, 0x00, {}},
};

static struct LCM_setting_table gray_3d_lut[] = {
	{0x57, 01, {0x00}},
};

static struct LCM_setting_table exit_aod[] = {
	{0x38, 01, {0x00}},
};

static struct LCM_setting_table lcm_aod_high_mode_with_lhbm_alpha[] = {
	{0x5F, 02, {0x00,0x40} },
	{0x51, 06, {0x00,0xF2,0x00,0x00,0x0F,0xFF}},
	//FPR_ALPHA_EN = 1, FPR_AUTO_ON = 1, FPR_ENABLE_CMD = 1
	{0xA9, 14, {0x02,0x00,0xB5,0x2C,0x2C,0x00,0x01,0x00,0x87,0x00,0x02,0x25,0x20,0x67}},
};

static struct LCM_setting_table lcm_aod_low_mode_with_lhbm_alpha[] = {
	{0x5F, 02, {0x00,0x40} },
	{0x51, 06, {0x00,0x14,0x00,0x00,0x01,0x55}},
	//FPR_ALPHA_EN = 1, FPR_AUTO_ON = 1, FPR_ENABLE_CMD = 1
	{0xA9, 14, {0x02,0x00,0xB5,0x2C,0x2C,0x00,0x01,0x00,0x87,0x00,0x02,0x25,0x20,0x67}},
};

static struct LCM_setting_table lhbm_normal_white_1200nit[] = {
	//cmdq2 page7
	{0x8B, 01, {0x10}},
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x08}},
	/*1200nit gir on*/
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x10}},
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x20}},
	{0xB6, 14, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x55}},
	{0xB8, 11, {0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x60}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x70}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x80}},
	{0xB8, 03, {0x00,0x10,0x00}},

	//FPR ON
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xD1, 06, {0x19,0x78,0x17,0xC4,0x1D,0x58}},

	//FPR_ALPHA_EN = 1, FPR_AUTO_ON = 1, FPR_ENABLE_CMD = 1
	{0xA9, 14, {0x02,0x00,0xB5,0x2C,0x2C,0x00,0x01,0x00,0x87,0x00,0x02,0x25,0x20,0x67}},
	//close change gray level by temperature
	{0x57, 01, {0x00}},
};

static struct LCM_setting_table lhbm_normal_white_250nit[] = {
	//cmdq2 page7
	{0x8B, 01, {0x10}},
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x08}},
	/*1200nit gir on*/
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x10}},
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x20}},
	{0xB6, 14, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x55}},
	{0xB8, 11, {0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x60}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x70}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x80}},
	{0xB8, 03, {0x00,0x10,0x00}},

	//FPR ON	
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xD1, 06, {0x1B,0xB4,0x19,0x8F,0x20,0x53}},

	//FPR_ALPHA_EN = 1, FPR_AUTO_ON = 1, FPR_ENABLE_CMD = 1
	{0xA9, 14, {0x02,0x00,0xB5,0x2C,0x2C,0x00,0x01,0x00,0x87,0x00,0x02,0x25,0x20,0x67}},
	//close change gray level by temperature
	{0x57, 01, {0x00}},
};

static struct LCM_setting_table lhbm_normal_green_500nit[] = {
	//cmdq2 page7
	{0x8B, 01, {0x10}},
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x08}},
	/*1200nit gir on*/
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x10}},
	{0xB6, 16, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x20}},
	{0xB6, 14, {0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x55}},
	{0xB8, 11, {0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x60}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x70}},
	{0xB8, 16, {0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10,0x00,0x10}},
	{0x6F, 01, {0x80}},
	{0xB8, 03, {0x00,0x10,0x00}},

	//FPR ON
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xD1, 06, {0x00,0x00,0x1F,0xD0,0x00,0x00}},

	//FPR_ALPHA_EN = 1, FPR_AUTO_ON = 1, FPR_ENABLE_CMD = 1
	{0xA9, 14, {0x02,0x00,0xB5,0x2C,0x2C,0x00,0x01,0x00,0x87,0x00,0x02,0x25,0x20,0x67}},
	//close change gray level by temperature
	{0x57, 01, {0x00}},
};

static struct LCM_setting_table lhbm_off[] = {
	//cmdq2 page7
	{0x8B, 01, {0x00}},
	{0xF0, 05, {0x55, 0xAA, 0x52, 0x08, 0x08}},
	/*1200nit gir on*/
	{0xB6, 16, {0x08,0x0B,0x08,0x0B,0x09,0x13,0x09,0x13,0x09,0x76,0x09,0x76,0x09,0xB8,0x09,0xB8}},
	{0x6F, 01, {0x10}},
	{0xB6, 16, {0x0A,0x23,0x0A,0x23,0x0A,0x44,0x0A,0x44,0x08,0xF2,0x08,0xF2,0x0A,0x65,0x0A,0x65}},
	{0x6F, 01, {0x20}},
	{0xB6, 14, {0x0B,0x8D,0x0B,0x8D,0x0C,0x32,0x0C,0x32,0x0F,0xFF,0x0F,0xFF,0x0F,0xFF}},
	{0x6F, 01, {0x55}},
	{0xB8, 11, {0x1F,0xD0,0x1F,0xD0,0x1C,0x33,0x1C,0x33,0x1B,0x0C,0x1B}},
	{0x6F, 01, {0x60}},
	{0xB8, 16, {0x0C,0x1A,0x54,0x1A,0x54,0x19,0x3E,0x19,0x3E,0x18,0xED,0x18,0xED,0x1C,0x9B,0x1C}},
	{0x6F, 01, {0x70}},
	{0xB8, 16, {0x9B,0x18,0x9E,0x18,0x9E,0x16,0x27,0x16,0x27,0x14,0xFC,0x14,0xFC,0x10,0x00,0x10}},
	{0x6F, 01, {0x80}},
	{0xB8, 03, {0x00,0x10,0x00}},
	
	//FPR OFF
	//FPR_ALPHA_EN = 1, FPR_AUTO_ON = 1, FPR_ENABLE_CMD = 1	
	{0xA9, 12, {0x02,0x00,0xB5,0x2C,0x2C,0x03,0x01,0x00,0x87,0x00,0x00,0x20}},
	//restore change gray level by temperature
	{0x57, 01, {0x00}},
};

static void push_table(struct lcm *ctx, struct LCM_setting_table *table, unsigned int count)
{
	unsigned int i, j;
	unsigned char temp[255] = {0};

	for (i = 0; i < count; i++) {
		unsigned int cmd;

		cmd = table[i].cmd;
		memset(temp, 0, sizeof(temp));
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			temp[0] = cmd;
			for (j = 0; j < table[i].count; j++)
				temp[j+1] = table[i].para_list[j];
			lcm_dcs_write(ctx, temp, table[i].count+1);
		}
	}
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
	static int time = 1;

	if (!ctx->prepared && time > 2)
		return 0;

	pr_info("%s, time = %d\n", __func__, time);

	mutex_lock(&ctx->panel_lock);
	if (time == 1) {
		push_table(ctx, lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table));
	} else {
		push_table(ctx, lcm_suspend_setting2, sizeof(lcm_suspend_setting2) / sizeof(struct LCM_setting_table));
	}
	++time;
	if (time == 3)
		time = 1;
	mutex_unlock(&ctx->panel_lock);

	ctx->error = 0;
	ctx->prepared = false;
	if (globle_mtk_dsi != NULL)
		globle_mtk_dsi->mi_cfg.last_bl_level = 0;

	atomic_set(&doze_enable, 0);
	globle_mtk_dsi->mi_cfg.lhbm_en = false;

	return 0;
}

static int lcm_check_panel_id(struct lcm *ctx)
{
	int ret = 0;
	u8 tx_buf[] = {0xDA};
	u8 rx_buf[16] = {0x00};
	struct mtk_ddic_dsi_msg cmds[] = {
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &tx_buf[0],
			.tx_len[0] = 1,
			.rx_buf[0] = &rx_buf[0],
			.rx_len[0] = 1,
		}
	};

	pr_info(" %s id=0x%x +\n", __func__,build_id);

	if (build_id)
		return ret;

	if (build_id == 0) {
		ret = mtk_ddic_dsi_read_cmd(&cmds[0]);
		build_id = rx_buf[0];
	}

	pr_info(" %s panel_id=0x%x -\n", __func__,build_id);
	return ret;
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s+\n", __func__);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}

	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(10 * 1000, (10 * 1000)+20);
	gpiod_set_value(ctx->reset_gpio, 0);
	usleep_range(1 * 1000, (1* 1000)+20);
	gpiod_set_value(ctx->reset_gpio, 1);
	usleep_range(11 * 1000, (11 * 1000)+20);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	pr_info("%s: dynamic_fps:%d, gir_status:%d\n", __func__,
		ctx->dynamic_fps, ctx->gir_status);

	if (ctx->dynamic_fps == 120  || ctx->dynamic_fps == 30) {
		if (init_setting_vdo[FPS_INIT_INDEX].cmd == 0x2F)
			init_setting_vdo[FPS_INIT_INDEX].para_list[0] = 0x00;
		else
			pr_info("%s: please check FPS_INIT_INDEX\n", __func__);
	} else if (ctx->dynamic_fps == 90) {
		if (init_setting_vdo[FPS_INIT_INDEX].cmd == 0x2F)
			init_setting_vdo[FPS_INIT_INDEX].para_list[0] = 0x01;
		else
			pr_info("%s: please check FPS_INIT_INDEX\n", __func__);
	} else if (ctx->dynamic_fps == 60) {
		if (init_setting_vdo[FPS_INIT_INDEX].cmd == 0x2F)
			init_setting_vdo[FPS_INIT_INDEX].para_list[0] = 0x02;
		else
		pr_info("%s: please check FPS_INIT_INDEX\n", __func__);
	}

	if (ctx->gir_status) {
		if (init_setting_vdo[GIR_INIT_INDEX1].cmd == 0x5F)
			init_setting_vdo[GIR_INIT_INDEX1].para_list[0] = 0x00;
		else
			pr_info("%s: please check GIR_INIT_INDEX1\n", __func__);
	} else {
		if (init_setting_vdo[GIR_INIT_INDEX1].cmd == 0x5F)
			init_setting_vdo[GIR_INIT_INDEX1].para_list[0] = 0x04;
		else
			pr_info("%s: please check GIR_INIT_INDEX1\n", __func__);
	}

	if(build_id == LCM_P1_0 || build_id == LCM_P1_1 || build_id == LCM_P2_0) {
		if (ctx->gray_level == TEMP_INDEX1_36) {
			if (init_setting_vdo[TEMP_INDEX].cmd == 0x57)
				init_setting_vdo[TEMP_INDEX].para_list[0] = 0x82;
		} else if (ctx->gray_level == TEMP_INDEX2_32) {
			if (init_setting_vdo[TEMP_INDEX].cmd == 0x57)
				init_setting_vdo[TEMP_INDEX].para_list[0] = 0x81;
		} else if (ctx->gray_level == TEMP_INDEX3_28) {
			if (init_setting_vdo[TEMP_INDEX].cmd == 0x57)
				init_setting_vdo[TEMP_INDEX].para_list[0] = 0x80;
		} else if (ctx->gray_level == TEMP_INDEX4_off) {
			if (init_setting_vdo[TEMP_INDEX].cmd == 0x57)
				init_setting_vdo[TEMP_INDEX].para_list[0] = 0x00;
		}
	}
	push_table(ctx, init_setting_vdo, sizeof(init_setting_vdo) / sizeof(struct LCM_setting_table));

	pr_info("%s-\n", __func__);
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);

	if (ctx->prepared)
		return 0;

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

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->prepared)
		return 0;

	ctx->reset_gpio = devm_gpiod_get_index(ctx->dev,
		"reset", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	udelay(1000);

	//VCI 3.0V -> 0
	lcm_panel_vci_disable(ctx->dev);
	udelay(1000);

	//DVDD 1.22V -> 0
	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
		"dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
	udelay(5000);

	//VDDIO 1.8V -> 0
	ctx->vddio18_gpio = devm_gpiod_get_index(ctx->dev,
		"vddio18", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddio18_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddio gpio %ld\n",
			__func__, PTR_ERR(ctx->vddio18_gpio));
		return PTR_ERR(ctx->vddio18_gpio);
	}
	gpiod_set_value(ctx->vddio18_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddio18_gpio);
	udelay(1000);

	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->prepared)
		return 0;

	pr_info("%s\n", __func__);

	//VDDIO 1.8V->1
	ctx->vddio18_gpio = devm_gpiod_get_index(ctx->dev,
		"vddio18", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddio18_gpio)) {
		dev_err(ctx->dev, "%s: cannot get vddio gpio %ld\n",
			__func__, PTR_ERR(ctx->vddio18_gpio));
		return PTR_ERR(ctx->vddio18_gpio);
	}
	gpiod_set_value(ctx->vddio18_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vddio18_gpio);
	udelay(1000);

	//DVDD 1.22V->1
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

	//VCI 3.0V->1
	lcm_panel_vci_enable(ctx->dev);
	udelay(12*1000);

	return 0;
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
	.clock = 246629,
	.hdisplay = 1220,
	.hsync_start = 1220 + 1738,		//HFP
	.hsync_end = 1220 + 1738 + 6,		//HSA
	.htotal = 1220 + 1738 + 6+ 6,		//HBP
	.vdisplay = 2712,
	.vsync_start = 2712 + 45,		//VFP
	.vsync_end = 2712 + 45 + 2,		//VSA
	.vtotal = 2712 + 45 + 2 + 9,		//VBP
};

static const struct drm_display_mode mode_60hz = {
	.clock = 425165,
	.hdisplay = 1220,
	.hsync_start = 1220 + 48,		//HFP
	.hsync_end = 1220 + 48 + 6,		//HSA
	.htotal = 1220 + 48 + 6 + 6,		//HBP
	.vdisplay = 2712,
	.vsync_start = 2712 + 2813,		//VFP
	.vsync_end = 2712 + 2813 + 2,		//VSA
	.vtotal = 2712 + 2813 + 2 + 9,		//VBP
};

static const struct drm_display_mode mode_90hz = {
	.clock = 425319,
	.hdisplay = 1220,
	.hsync_start = 1220 + 48,		//HFP
	.hsync_end = 1220 + 48 + 6,		//HSA
	.htotal = 1220 + 48 + 6 + 6,		//HBP
	.vdisplay = 2712,
	.vsync_start = 2712 + 969,		//VFP
	.vsync_end = 2712 + 969 + 2,		//VSA
	.vtotal = 2712 + 969 + 2 + 9,		//VBP
};

static const struct drm_display_mode mode_120hz = {
	.clock = 425165,
	.hdisplay = 1220,
	.hsync_start = 1220 + 48,		//HFP
	.hsync_end = 1220 + 48 + 6,		//HSA
	.htotal = 1220 + 48 + 6 + 6,		//HBP
	.vdisplay = 2712,
	.vsync_start = 2712 + 45,		//VFP
	.vsync_end = 2712 + 45 + 2,		//VSA
	.vtotal = 2712 + 45 + 2 + 9,		//VBP
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params_30hz = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0xDC,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
#ifdef CONFIG_MI_DISP_FOD_SYNC
  	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 30,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 3,
		.short_dfps_cmds_start_index = 0,
		.long_dfps_cmds_counts = 3,
		.dfps_cmd_table[0] = {0, 3, {0x5F,0x00,0x40} },
		.dfps_cmd_table[1] = {0, 7, {0x51,0x00,0xF2,0x00,0x00,0x0F,0xFF} },
		.dfps_cmd_table[2] = {0, 2, {0x39,0x00} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static struct mtk_panel_params ext_params_60hz = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
#ifdef CONFIG_MI_DISP_FOD_SYNC
  	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 1,
		.long_dfps_cmds_counts = 2,
		.dfps_cmd_table[0] = {0, 2, {0x38, 0x00} },
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x02} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static struct mtk_panel_params ext_params_90hz = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 1,
		.long_dfps_cmds_counts = 2,
		.dfps_cmd_table[0] = {0, 2, {0x38, 0x00} },
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x01} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

static struct mtk_panel_params ext_params_120hz = {
	.lcm_index = 0,
	.pll_clk = DATA_RATE / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.is_cphy = 0,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
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
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
		.cmds_counts_switch_fps = 30,
		.short_dfps_cmds_counts = 1,
		.short_dfps_cmds_start_index = 1,
		.long_dfps_cmds_counts = 2,
		.dfps_cmd_table[0] = {0, 2, {0x38, 0x00} },
		.dfps_cmd_table[1] = {0, 2, {0x2F, 0x00} },
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
};

struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
	unsigned int mode)
{
	struct drm_display_mode *m = NULL;
	unsigned int i = 0;

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
	int dst_fps = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;

	if (dst_fps == 30)
		*ext_param = &ext_params_30hz;
	else if (dst_fps == 60)
		*ext_param = &ext_params_60hz;
	else if (dst_fps == 90)
		*ext_param = &ext_params_90hz;
	else if (dst_fps == 120)
		*ext_param = &ext_params_120hz;
	else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);

	pr_info("%s drm_mode_vrefresh = %d, m->hdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_dst), m_dst->hdisplay);
	dynamic_change_fps_going = true;

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;

	globle_mtk_dsi->mi_cfg.dynamic_fps = dst_fps;

	if (dst_fps == 30) {
		if (panel_ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM) {
			ext_params_30hz.dyn_fps.dfps_cmd_table[1].para_list[1] = (doze_hbm_dbv_level >>8) & 0xFF;
			ext_params_30hz.dyn_fps.dfps_cmd_table[1].para_list[2] = doze_hbm_dbv_level & 0xFF;
			ext_params_30hz.dyn_fps.dfps_cmd_table[1].para_list[5] = 0x0F;
			ext_params_30hz.dyn_fps.dfps_cmd_table[1].para_list[6] = 0xFF;
		} else if (panel_ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM) {
			ext_params_30hz.dyn_fps.dfps_cmd_table[1].para_list[1] = (doze_lbm_dbv_level >>8) & 0xFF;
			ext_params_30hz.dyn_fps.dfps_cmd_table[1].para_list[2] = doze_lbm_dbv_level & 0xFF;
			ext_params_30hz.dyn_fps.dfps_cmd_table[1].para_list[5] = 0x01;
			ext_params_30hz.dyn_fps.dfps_cmd_table[1].para_list[6] = 0x55;
		}
		ext->params = &ext_params_30hz;
	} else if (dst_fps == 60) {
		ext->params = &ext_params_60hz;
	} else if (dst_fps == 90) {
		ext->params = &ext_params_90hz;
	} else if (dst_fps == 120) {
		ext->params = &ext_params_120hz;
	} else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

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

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0x00, 0x00};
	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;
	struct lcm *ctx = panel_to_lcm(this_panel);

	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
		mtk_dsi->mi_cfg.last_no_zero_bl_level = level;
	}

	if (!cb)
		return -1;

	if (atomic_read(&doze_enable)) {
		pr_info("%s: Return it when aod on, %d %d %d!\n", __func__, level, bl_tb0[1], bl_tb0[2]);
		return 0;
	}

	pr_info("%s: level %d = 0x%02X, 0x%02X\n", __func__, level, bl_tb0[1], bl_tb0[2]);

	mutex_lock(&ctx->panel_lock);
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	mutex_unlock(&ctx->panel_lock);

	if (level != 0)
		last_non_zero_bl_level = level;

	mtk_dsi->mi_cfg.last_bl_level = level;

	return 0;
}

void get_changed_fps(unsigned int new_fps)
{
	panel_ctx->dynamic_fps = new_fps;
	dynamic_change_fps_going = false;
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
	mutex_unlock(&ctx->panel_lock);
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x01);
	mutex_unlock(&ctx->panel_lock);
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
	mutex_unlock(&ctx->panel_lock);
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);
	bool isFpsChange = false;

	if (cur_mode == dst_mode)
		return ret;

	isFpsChange = drm_mode_vrefresh(m_dst) == drm_mode_vrefresh(m_cur)? false: true;

	pr_info("%s isFpsChange = %d, dst_mode vrefresh = %d, cur_mode vrefresh = %d,, vdisplay = %d, hdisplay = %d\n",
			__func__, isFpsChange, drm_mode_vrefresh(m_dst), drm_mode_vrefresh(m_cur), m_dst->vdisplay, m_dst->hdisplay);

	if (isFpsChange) {
		if (drm_mode_vrefresh(m_dst) == 60) {
			mode_switch_to_60(panel, stage);
		} else if (drm_mode_vrefresh(m_dst) == 90) {
			mode_switch_to_90(panel, stage);
		} else if (drm_mode_vrefresh(m_dst) == 120 || drm_mode_vrefresh(m_dst) == 30) {
			mode_switch_to_120(panel, stage);
		} else
			ret = 1;
	}

	return ret;
}

static struct LCM_setting_table lcm_aod_high_mode[] = {
	{0x5F, 02, {0x00,0x40} },
	{0x51, 06, {0x00,0xF2,0x00,0x00,0x0F,0xFF}},
};

static struct LCM_setting_table lcm_aod_low_mode[] = {
	{0x5F, 02, {0x00,0x40} },
	{0x51, 06, {0x00,0x14,0x00,0x00,0x01,0x55}},
};


static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
	int ret = 0;
	struct lcm *ctx;

	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}

	ctx = panel_to_lcm(panel);

	pr_info("%s set_doze_brightness state = %d",__func__, doze_brightness);

	if (DOZE_BRIGHTNESS_LBM  == doze_brightness || DOZE_BRIGHTNESS_HBM  == doze_brightness) {
		if (DOZE_BRIGHTNESS_LBM  == doze_brightness)
			ret = mi_disp_panel_ddic_send_cmd(lcm_aod_low_mode, ARRAY_SIZE(lcm_aod_low_mode), false, false);
		else if (DOZE_BRIGHTNESS_HBM == doze_brightness)
			ret = mi_disp_panel_ddic_send_cmd(lcm_aod_high_mode, ARRAY_SIZE(lcm_aod_high_mode), false, false);
		if (ctx->dynamic_fps != 30)
			dynamic_change_fps_going = true;
		else
			dynamic_change_fps_going = false;
		atomic_set(&doze_enable, 1);
	}

	if (DOZE_TO_NORMAL == doze_brightness) {
		if (ctx->dynamic_fps == 30)
			dynamic_change_fps_going = true;
		else
			dynamic_change_fps_going = false;
		atomic_set(&doze_enable, 0);
	}

	ctx->doze_brightness_state = doze_brightness;
	pr_info("%s set doze_brightness %d end -\n", __func__, doze_brightness);

	return ret;
}

static int panel_get_doze_brightness(struct drm_panel *panel, u32 *doze_brightness)
{
	int count = 0;
	struct lcm *ctx = panel_to_lcm(panel);

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	*doze_brightness = ctx->doze_brightness_state;

	pr_info("%s get doze_brightness %d end -\n", __func__, *doze_brightness);

	return count;
}

#ifdef CONFIG_MI_DISP
static void lcm_esd_restore_backlight(struct drm_panel *panel)
{
	char bl_tb0[] = {0x51, 0x00, 0x00};
	struct lcm *ctx = panel_to_lcm(panel);

	bl_tb0[1] = (last_non_zero_bl_level >> 8) & 0xFF;
	bl_tb0[2] = last_non_zero_bl_level & 0xFF;

	pr_info("%s: restore to level = %d\n", __func__, last_non_zero_bl_level);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	mutex_unlock(&ctx->panel_lock);

	return;
}

static unsigned int bl_level = 2047;
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
	}
	pr_err("lcm_setbacklight_control backlight %d\n", level);

	return 0;
}

int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	/* Read 8 bytes from 0xA1 register
	 * 	BIT[0-1] = Lux
	 * 	BIT[2-4] = Wx
	 * 	BIT[5-7] = Wy */
	static uint16_t lux = 0, wx = 0, wy = 0;
	int i, ret = 0, count = 0;
	struct lcm *ctx = NULL;
	u8 tx_buf[] = {0xA3};
	u8 rx_buf[8] = {0x00};
	struct mtk_ddic_dsi_msg cmds[] = {
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &tx_buf[0],
			.tx_len[0] = 1,
			.rx_buf[0] = &rx_buf[0],
			.rx_len[0] = 8,
		},
	};

	/* try to get wp info from cache */
	if (lux > 0 && wx > 0 && wy > 0) {
		pr_info("%s: got wp info from cache\n", __func__);
		goto cache;
	}

	/* try to get wp info from cmdline */
	if (sscanf(oled_wp_cmdline, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
			&rx_buf[0], &rx_buf[1], &rx_buf[2], &rx_buf[3],
			&rx_buf[4], &rx_buf[5]) == 8) {

		lux = rx_buf[0] << 8 | rx_buf[1];
		wx = rx_buf[2] << 8 | rx_buf[3];
		wy = rx_buf[4] << 8 | rx_buf[5];
		if (lux > 0 && wx > 0 && wy > 0) {
			pr_info("%s: got wp info from cmdline\n", __func__);
			goto done;
		}
	}

	/* try to get wp info from panel register */
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}

	ctx = panel_to_lcm(panel);
	if (!ctx || !ctx->enabled) {
		pr_err("%s: ctx is NULL or panel isn't enabled\n", __func__);
		goto err;
	}

	memset(rx_buf, 0, sizeof(rx_buf));
	for (i = 0; i < sizeof(cmds)/sizeof(struct mtk_ddic_dsi_msg); ++i) {
		ret |= mtk_ddic_dsi_read_cmd(&cmds[i]);
	}

	if (ret != 0) {
		pr_err("%s: failed to read ddic register\n", __func__);
		memset(rx_buf, 0, sizeof(rx_buf));
		goto err;
	}

	/* rx_buf[0-1] is lux(HEX), rx_buf[2-4] is wx(DEC), rx_buf[5-7] is wy(DEC) */
	lux = rx_buf[0] << 8 | rx_buf[1];
	wx = rx_buf[2] * 100 + rx_buf[3] * 10 + rx_buf[4];
	wy = rx_buf[5] * 100 + rx_buf[6] * 10 + rx_buf[7];

cache:
	rx_buf[0]  = (lux >> 8) & 0x00ff;
	rx_buf[1] = lux & 0x00ff;

	rx_buf[2] = (wx >> 8) & 0x00ff;
	rx_buf[3] = wx & 0x00ff;

	rx_buf[4] = (wy >> 8) & 0x00ff;
	rx_buf[5] = wy & 0x00ff;
done:
	count = snprintf(buf, size, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
			rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3],
			rx_buf[4], rx_buf[5]);

	pr_info("%s: Lux=0x%04hx, Wx=0x%04hx, Wy=0x%04hx\n", __func__, lux, wx, wy);
err:
	pr_info("%s: -\n", __func__);
	return count;
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

static int panel_set_gir_on(struct drm_panel *panel)
{
	struct LCM_setting_table gir_on_set[] = {
		{0x5F, 2, {0x00,0x40} },
	};
	struct lcm *ctx;
	int ret = 0;

	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}

	ctx = panel_to_lcm(panel);
	ctx->gir_status = 1;
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
		mi_disp_panel_ddic_send_cmd(gir_on_set, ARRAY_SIZE(gir_on_set), false, false);
	}

err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_set_gir_off(struct drm_panel *panel)
{
	struct LCM_setting_table gir_off_set[] = {
		{0x5F, 2, {0x04,0x40} },
	};
	struct lcm *ctx;
	int ret = 0;

	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}

	ctx = panel_to_lcm(panel);
	ctx->gir_status = 0;
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
		mi_disp_panel_ddic_send_cmd(gir_off_set, ARRAY_SIZE(gir_off_set), false, false);
	}

err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_get_gir_status(struct drm_panel *panel)
{
	struct lcm *ctx;

	if (!panel) {
		pr_err("%s; panel is NULL\n", __func__);
		return -1;
	}

	ctx = panel_to_lcm(panel);

	return ctx->gir_status;
}

static bool get_panel_initialized(struct drm_panel *panel)
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

static int panel_get_factory_max_brightness(struct drm_panel *panel, u32 *max_brightness_clone)
{
	struct lcm *ctx;

	pr_info("%s +\n", __func__);
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	*max_brightness_clone = ctx->factory_max_brightness;

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

static int panel_fod_lhbm_init (struct mtk_dsi* dsi)
{
	if (!dsi) {
		pr_info("invalid dsi point\n");
		return -1;
	}
	globle_mtk_dsi = dsi;
	pr_info("panel_fod_lhbm_init enter\n");
	dsi->display_type = "primary";
	dsi->mi_cfg.lhbm_ui_ready_delay_frame = 3;
	dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod = 3;
	dsi->mi_cfg.local_hbm_enabled = 1;
	return 0;
}

static int panel_hbm_fod_control(struct drm_panel *panel, bool en)
{
	struct lcm *ctx;
	int ret = -1;
	struct LCM_setting_table hbm_fod_on_cmd[] = {
		{0x51, 02, {0x0F,0xFF}},
	};
	struct LCM_setting_table hbm_fod_off_cmd[] = {
		{0x51, 02, {0x07,0xFF}},
	};

	pr_info("%s: +\n", __func__);
 	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}
 	ctx = panel_to_lcm(panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}
 	if (en) {
		ret = mi_disp_panel_ddic_send_cmd(hbm_fod_on_cmd, ARRAY_SIZE(hbm_fod_on_cmd), false, false);
	} else {
		ret = mi_disp_panel_ddic_send_cmd(hbm_fod_off_cmd, ARRAY_SIZE(hbm_fod_on_cmd), false, false);
	}
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static void mi_parse_cmdline_perBL(struct LHBM_WHITEBUF * lhbm_whitebuf) {
	int i = 0;
	static u8 lhbm_cmdbuf[18] = {0};

	pr_info("mi_parse_cmdline_perBL enter\n");

	if(!lhbm_w1200_update_flag && !lhbm_w250_update_flag && !lhbm_g500_update_flag) {
		pr_info("don't need update white rgb config");
		return;
	}

	if (lhbm_whitebuf == NULL) {
		pr_err("lhbm_status == NULL\n");
		return;
	}

	for (i = 0; i < 18; i++) {
		sscanf(oled_lhbm_cmdline + 2 * i, "%02hhx", &lhbm_cmdbuf[i]);
	}

	for (i = 0; i < 6; i++){
		lhbm_whitebuf->lhbm_1200[i] = lhbm_cmdbuf[i];
		lhbm_whitebuf->lhbm_500[i] = lhbm_cmdbuf[i+6];
		lhbm_whitebuf->lhbm_250[i] = lhbm_cmdbuf[i+12];
	}

	pr_info("lhbm_1200 \n");
	for (i = 0; i < 6; i++){
		lhbm_normal_white_1200nit[16].para_list[i] = lhbm_whitebuf->lhbm_1200[i];
		pr_info("0x%02hhx",lhbm_whitebuf->lhbm_1200[i]);
	}

	pr_info("lhbm_250 \n");
	for (i = 0; i < 6; i++){
		lhbm_normal_white_250nit[16].para_list[i] = lhbm_whitebuf->lhbm_250[i];
		pr_info("0x%02hhx ",lhbm_whitebuf->lhbm_250[i]);
	}

	pr_info("lhbm_500 \n");
	for (i = 0; i < 2; i++){
		lhbm_normal_green_500nit[16].para_list[i] = lhbm_whitebuf->lhbm_500[i];
		pr_info("0x%02hhx ",lhbm_whitebuf->lhbm_500[i]);
	}

	lhbm_w1200_readbackdone = true;
	lhbm_w250_readbackdone = true;
	lhbm_g500_readbackdone = true;
	lhbm_w1200_update_flag = false;
	lhbm_w250_update_flag = false;
	lhbm_g500_update_flag =false;

	return;
}

static int mi_disp_panel_update_lhbm_A9reg(struct mtk_dsi * dsi, enum lhbm_cmd_type type, int flat_mode, int bl_level)
{
	u8 alpha_buf[2] = {0};

	if(!dsi)
		return -EINVAL;

	if(!lhbm_w1200_readbackdone ||
		!lhbm_w250_readbackdone ||
		!lhbm_g500_readbackdone) {
		pr_info("mi_disp_panel_update_lhbm_white_param cmdline_lhbm:%s\n", oled_lhbm_cmdline);
		mi_parse_cmdline_perBL(&lhbm_whitebuf);
	}

	if(flat_mode)
		pr_info("lhbm update 0xA9, lhbm_cmd_type:%d, flat_mode:%d, bl_level = %d, alpha = %d\n", type, flat_mode, bl_level, giron_alpha_set[bl_level]);
	else
		pr_info("lhbm update 0xA9, lhbm_cmd_type:%d, flat_mode:%d, bl_level = %d, alpha = %d\n", type, flat_mode, bl_level, giroff_alpha_set[bl_level]);


	switch (type) {
	case TYPE_WHITE_1200:
		if (flat_mode) {
			alpha_buf[0] = (giron_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giron_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_white_1200nit[17].para_list[12] = alpha_buf[0];
			lhbm_normal_white_1200nit[17].para_list[13] = alpha_buf[1];
		} else {
			alpha_buf[0] = (giroff_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giroff_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_white_1200nit[17].para_list[12] = alpha_buf[0];
			lhbm_normal_white_1200nit[17].para_list[13] = alpha_buf[1];
		}
		break;
	case TYPE_WHITE_250:
		if (flat_mode) {
			alpha_buf[0] = (giron_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giron_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_white_250nit[17].para_list[12] = alpha_buf[0];
			lhbm_normal_white_250nit[17].para_list[13] = alpha_buf[1];
		} else {
			alpha_buf[0] = (giroff_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giroff_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_white_250nit[17].para_list[12] = alpha_buf[0];
			lhbm_normal_white_250nit[17].para_list[13] = alpha_buf[1];
		}
		break;
	case TYPE_GREEN_500:
		if (flat_mode) {
			alpha_buf[0] = (giron_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giron_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_green_500nit[17].para_list[12] = alpha_buf[0];
			lhbm_normal_green_500nit[17].para_list[13] = alpha_buf[1];
		} else {
			alpha_buf[0] = (giroff_alpha_set[bl_level] >> 8) & 0xff;
			alpha_buf[1] = giroff_alpha_set[bl_level] & 0xff;
			pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
					type, alpha_buf[0],  alpha_buf[1]);
			lhbm_normal_green_500nit[17].para_list[12] = alpha_buf[0];
			lhbm_normal_green_500nit[17].para_list[13] = alpha_buf[1];
		}
		pr_info("green 500 update flat mode off 120\n");
		break;
	default:
		pr_err("unsuppport cmd \n");
	return -EINVAL;
	}

	return 0;
}

static int panel_set_lhbm_fod(struct mtk_dsi *dsi, enum local_hbm_state lhbm_state)
{
	struct lcm *ctx = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	int bl_level;
	int flat_mode;
	int bl_level_doze = doze_hbm_dbv_level;
	int i = 0;

	if (!dsi || !dsi->panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return -1;
	}
	ctx = panel_to_lcm(dsi->panel);
	mi_cfg = &dsi->mi_cfg;
	bl_level = mi_cfg->last_bl_level;
	flat_mode = mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE];
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		return -1;
	}
	if (atomic_read(&doze_enable) &&
		ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
			bl_level_doze = doze_lbm_dbv_level;
	else if (atomic_read(&doze_enable) &&
		ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
			bl_level_doze = doze_hbm_dbv_level;
	else
			bl_level_doze = mi_cfg->last_no_zero_bl_level;
	pr_info("%s local hbm_state:%d \n",__func__, lhbm_state);
	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL:
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
		pr_info("LOCAL_HBM_OFF, ctx->gray_level=0x%02x\n", ctx->gray_level);
		mi_cfg->lhbm_en = false;
		if (ctx->gray_level == TEMP_INDEX1_36) {
			lhbm_off[16].para_list[0] = 0x82;
		} else if (ctx->gray_level == TEMP_INDEX2_32) {
			lhbm_off[16].para_list[0] = 0x81;
		} else if (ctx->gray_level == TEMP_INDEX3_28) {
			lhbm_off[16].para_list[0] = 0x80;
		} else if (ctx->gray_level == TEMP_INDEX4_off ) {
			lhbm_off[16].para_list[0] = 0x00;
		}
		mi_disp_panel_ddic_send_cmd_with_speed(lhbm_off,
			ARRAY_SIZE(lhbm_off), false, true, MIPI_DSI_MSG_USE_LPM);

		ctx->lhbm_en = false;
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
		pr_info("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE, bl_level=%d, ctx->gray_level=0x%02x\n", bl_level, ctx->gray_level);
		mi_cfg->lhbm_en = false;
		if (ctx->gray_level == TEMP_INDEX1_36) {
			lhbm_off[16].para_list[0] = 0x82;
		} else if (ctx->gray_level == TEMP_INDEX2_32) {
			lhbm_off[16].para_list[0] = 0x81;
		} else if (ctx->gray_level == TEMP_INDEX3_28) {
			lhbm_off[16].para_list[0] = 0x80;
		} else if (ctx->gray_level == TEMP_INDEX4_off ) {
			lhbm_off[16].para_list[0] = 0x00;
		}
		mi_disp_panel_ddic_send_cmd_with_speed(lhbm_off, ARRAY_SIZE(lhbm_off),
			false, true, MIPI_DSI_MSG_USE_LPM);

		ctx->lhbm_en = false;
		break;
	case LOCAL_HBM_NORMAL_WHITE_750NIT:
		break;
	case LOCAL_HBM_NORMAL_WHITE_500NIT:
		break;
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
		pr_info("LOCAL_HBM_NORMAL_WHITE_250NIT\n");
		mi_cfg->lhbm_en = true;
		if (atomic_read(&doze_enable)) {
			mutex_unlock(&dsi->dsi_lock);
			if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
				mi_cfg->last_no_zero_bl_level = doze_hbm_dbv_level;
			else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
				mi_cfg->last_no_zero_bl_level = doze_lbm_dbv_level;
			mi_dsi_panel_set_doze_brightness(dsi, DOZE_TO_NORMAL);
			mutex_lock(&dsi->dsi_lock);
			for (i = 0; i < 12; i ++) {
				if (ctx->dynamic_fps != 30 && dynamic_change_fps_going == false)
					break;
				else
					msleep(5);
			}
			pr_info("LOCAL_HBM_NORMAL_WHITE_250NIT, wait  %dms, ctx->dynamic_fps = %d\n", i*5, ctx->dynamic_fps);
		}
		if (ctx->dynamic_fps == 30) {
			pr_info("fps may incorrect, exit 30hz aod first\n");
			mi_disp_panel_ddic_send_cmd_with_speed(exit_aod,
				ARRAY_SIZE(exit_aod), false, true, 0);
		}
		if(bl_level > normal_max_bl)
			bl_level = normal_max_bl;
		mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_WHITE_250, flat_mode, bl_level);
		mi_disp_panel_ddic_send_cmd_with_speed(lhbm_normal_white_250nit,
			ARRAY_SIZE(lhbm_normal_white_250nit), false, true, MIPI_DSI_MSG_USE_LPM);

		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
		mi_cfg->lhbm_en = true;
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		pr_info("LOCAL_HBM_NORMAL_GREEN_500NIt\n");

		if(bl_level > normal_max_bl)
			bl_level = normal_max_bl;
		mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_GREEN_500, flat_mode, bl_level);
		mi_disp_panel_ddic_send_cmd_with_speed(lhbm_normal_green_500nit,
			ARRAY_SIZE(lhbm_normal_green_500nit), false, true, MIPI_DSI_MSG_USE_LPM);

		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
		pr_info("LOCAL_HBM_NORMAL_WHITE_1200NIT\n");
		mi_cfg->lhbm_en = true;
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		if (atomic_read(&doze_enable)) {
			mutex_unlock(&dsi->dsi_lock);
			if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
				mi_cfg->last_no_zero_bl_level = doze_hbm_dbv_level;
			else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
				mi_cfg->last_no_zero_bl_level = doze_lbm_dbv_level;
			mi_dsi_panel_set_doze_brightness(dsi, DOZE_TO_NORMAL);
			mutex_lock(&dsi->dsi_lock);
			for (i = 0; i < 12; i ++) {
				if (ctx->dynamic_fps != 30 && dynamic_change_fps_going == false)
					break;
				else
					msleep(5);
			}
			pr_info("LOCAL_HBM_NORMAL_WHITE_1200NIT, wait  %dms, ctx->dynamic_fps = %d\n", i*5, ctx->dynamic_fps);
		}
		if (ctx->dynamic_fps == 30) {
			pr_info("fps may incorrect, exit 30hz aod first\n");
			mi_disp_panel_ddic_send_cmd_with_speed(exit_aod,
				ARRAY_SIZE(exit_aod), false, true, 0);
		}
		if(bl_level > normal_max_bl)
			bl_level = normal_max_bl;
		mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_WHITE_1200, flat_mode, bl_level);
		mi_disp_panel_ddic_send_cmd_with_speed(lhbm_normal_white_1200nit,
			ARRAY_SIZE(lhbm_normal_white_1200nit), false, true, MIPI_DSI_MSG_USE_LPM);

		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		mi_cfg->lhbm_en = true;
		pr_info("LOCAL_HBM_HLPM_WHITE_250NIT, ctx->dynamic_fps = %d\n", ctx->dynamic_fps);
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		if (atomic_read(&doze_enable)) {
			mutex_unlock(&dsi->dsi_lock);
			if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
				mi_cfg->last_no_zero_bl_level = doze_hbm_dbv_level;
			else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
				mi_cfg->last_no_zero_bl_level = doze_lbm_dbv_level;
			mi_dsi_panel_set_doze_brightness(dsi, DOZE_TO_NORMAL);
			mutex_lock(&dsi->dsi_lock);
		}

		if(bl_level > normal_max_bl)
			bl_level = normal_max_bl;
		mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_WHITE_250, flat_mode, bl_level_doze);
		for (i = 0; i < 12; i ++) {
			if (ctx->dynamic_fps != 30 && dynamic_change_fps_going == false)
				break;
			else
				msleep(5);
		}
		pr_info("LOCAL_HBM_NORMAL_WHITE_250NIT, wait  %dms, ctx->dynamic_fps = %d\n", i*5, ctx->dynamic_fps);
		if (ctx->dynamic_fps == 30) {
			pr_info("fps may incorrect, exit 30hz aod first\n");
			mi_disp_panel_ddic_send_cmd_with_speed(exit_aod,
				ARRAY_SIZE(exit_aod), false, true, 0);
		}
		mi_disp_panel_ddic_send_cmd_with_speed(lhbm_normal_white_250nit,
			ARRAY_SIZE(lhbm_normal_white_250nit), false, true, MIPI_DSI_MSG_USE_LPM);

		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
		mi_cfg->lhbm_en = true;
		pr_info("LOCAL_HBM_HLPM_WHITE_1200NIT, ctx->dynamic_fps = %d, doze_enable=%d\n",
			ctx->dynamic_fps, atomic_read(&doze_enable));
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		if (atomic_read(&doze_enable)) {
			mutex_unlock(&dsi->dsi_lock);
			if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
				mi_cfg->last_no_zero_bl_level = doze_hbm_dbv_level;
			else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
				mi_cfg->last_no_zero_bl_level = doze_lbm_dbv_level;
			mi_dsi_panel_set_doze_brightness(dsi, DOZE_TO_NORMAL);
			mutex_lock(&dsi->dsi_lock);
		}

		if(bl_level > normal_max_bl)
			bl_level = normal_max_bl;
		mi_disp_panel_update_lhbm_A9reg(dsi, TYPE_WHITE_1200, flat_mode, bl_level_doze);
		for (i = 0; i < 12; i ++) {
			if (ctx->dynamic_fps != 30 && dynamic_change_fps_going == false)
				break;
			else
				msleep(5);
		}
		pr_info("LOCAL_HBM_HLPM_WHITE_1200NIT, wait  %dms, ctx->dynamic_fps = %d\n", i*5, ctx->dynamic_fps);
		if (ctx->dynamic_fps == 30) {
			pr_info("fps may incorrect, exit 30hz aod first\n");
			mi_disp_panel_ddic_send_cmd_with_speed(exit_aod,
				ARRAY_SIZE(exit_aod), false, true, 0);
		}
		mi_disp_panel_ddic_send_cmd_with_speed(lhbm_normal_white_1200nit,
			ARRAY_SIZE(lhbm_normal_white_1200nit), false, true, MIPI_DSI_MSG_USE_LPM);

		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_OFF_TO_HLPM:
		pr_info("LOCAL_HBM_OFF_TO_HLPM\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		break;
	case LOCAL_HBM_OFF_TO_LLPM:
		pr_info("LOCAL_HBM_OFF_TO_LLPM\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		break;
	default:
		pr_info("invalid local hbm value\n");
		break;
	}

	return 0;
}

static int panel_set_gray_by_temperature (struct drm_panel *panel, int level)
{
	int ret = 0;
	struct lcm *ctx;

	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}

	ctx = panel_to_lcm(panel);
	if (!ctx->enabled) {
		ctx->gray_level = level;
		ret = -1;
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}

	lcm_check_panel_id(ctx);

	if(build_id == LCM_P0_1 || build_id == LCM_P0_0) {
		pr_err("%s: panel is not p1.0\n", __func__);
		ret = -1;
		goto err;
	}

	pr_info("%s: level = %x\n", __func__, level);

	ctx->gray_level = level;

	if (level == TEMP_INDEX1_36) {
		gray_3d_lut[0].para_list[0] = 0x82;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut, ARRAY_SIZE(gray_3d_lut), false, false);
	} else if (level == TEMP_INDEX2_32) {
		gray_3d_lut[0].para_list[0] = 0x81;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut, ARRAY_SIZE(gray_3d_lut), false, false);
	} else if (level == TEMP_INDEX3_28) {
		gray_3d_lut[0].para_list[0] = 0x80;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut, ARRAY_SIZE(gray_3d_lut), false, false);
	} else if (level == TEMP_INDEX4_off ) {
		gray_3d_lut[0].para_list[0] = 0x00;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut, ARRAY_SIZE(gray_3d_lut), false, false);
	}

err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static struct LCM_setting_table lcm_backlight_off[] = {
	{0x51, 06, {0x00,0x00,0x00,0x00,0x00,0x00}},
};

static int panel_set_only_aod_backlight(struct drm_panel *panel, int doze_brightness)
{
	int ret = 0;
	struct lcm *ctx;

	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}

	ctx = panel_to_lcm(panel);

	pr_info("%s set_doze_brightness state = %d",__func__, doze_brightness);

	if (DOZE_BRIGHTNESS_LBM  == doze_brightness || DOZE_BRIGHTNESS_HBM  == doze_brightness) {
		if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
			globle_mtk_dsi->mi_cfg.last_bl_level = doze_lbm_dbv_level;
			if (globle_mtk_dsi->mi_cfg.lhbm_en) {
				if (globle_mtk_dsi->mi_cfg.feature_val[DISP_FEATURE_FLAT_MODE]) {
					lcm_aod_low_mode_with_lhbm_alpha[2].para_list[12] =
						(giron_alpha_set[globle_mtk_dsi->mi_cfg.last_bl_level] >> 8) & 0xff;
					lcm_aod_low_mode_with_lhbm_alpha[2].para_list[13] =
						giron_alpha_set[globle_mtk_dsi->mi_cfg.last_bl_level] & 0xff;
				} else {
					lcm_aod_low_mode_with_lhbm_alpha[2].para_list[12] =
						(giroff_alpha_set[globle_mtk_dsi->mi_cfg.last_bl_level] >> 8) & 0xff;
					lcm_aod_low_mode_with_lhbm_alpha[2].para_list[13] =
						giroff_alpha_set[globle_mtk_dsi->mi_cfg.last_bl_level] & 0xff;
				}
				mi_disp_panel_ddic_send_cmd(lcm_aod_low_mode_with_lhbm_alpha,
					ARRAY_SIZE(lcm_aod_low_mode_with_lhbm_alpha), false, false);
			} else {
				ret = mi_disp_panel_ddic_send_cmd(lcm_aod_low_mode, ARRAY_SIZE(lcm_aod_low_mode), false, false);
			}
		} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
			globle_mtk_dsi->mi_cfg.last_bl_level = doze_hbm_dbv_level;
			if (globle_mtk_dsi->mi_cfg.lhbm_en) {
				if (globle_mtk_dsi->mi_cfg.feature_val[DISP_FEATURE_FLAT_MODE]) {
					lcm_aod_high_mode_with_lhbm_alpha[2].para_list[12] =
						(giron_alpha_set[globle_mtk_dsi->mi_cfg.last_bl_level] >> 8) & 0xff;
					lcm_aod_high_mode_with_lhbm_alpha[2].para_list[13] =
						giron_alpha_set[globle_mtk_dsi->mi_cfg.last_bl_level] & 0xff;
				} else {
					lcm_aod_high_mode_with_lhbm_alpha[2].para_list[12] =
						(giroff_alpha_set[globle_mtk_dsi->mi_cfg.last_bl_level] >> 8) & 0xff;
					lcm_aod_high_mode_with_lhbm_alpha[2].para_list[13] =
						giroff_alpha_set[globle_mtk_dsi->mi_cfg.last_bl_level] & 0xff;
				}
				mi_disp_panel_ddic_send_cmd(lcm_aod_high_mode_with_lhbm_alpha,
					ARRAY_SIZE(lcm_aod_high_mode_with_lhbm_alpha), false, false);
			} else {
				ret = mi_disp_panel_ddic_send_cmd(lcm_aod_high_mode, ARRAY_SIZE(lcm_aod_high_mode), false, false);
			}
		}

	} else if (doze_brightness == 0) {
		ret = mi_disp_panel_ddic_send_cmd(lcm_backlight_off, ARRAY_SIZE(lcm_backlight_off), false, false);
		globle_mtk_dsi->mi_cfg.last_bl_level = 0;
	}

	ctx->doze_brightness_state = doze_brightness;
	pr_info("%s set doze_brightness %d end -\n", __func__, doze_brightness);
	return ret;
}

#endif

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.mode_switch = mode_switch,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.panel_poweron = lcm_panel_poweron,
	.panel_poweroff = lcm_panel_poweroff,
#ifdef CONFIG_MI_DISP
	.esd_restore_backlight = lcm_esd_restore_backlight,
	.setbacklight_control = lcm_setbacklight_control,
	.get_wp_info = panel_get_wp_info,
	.get_panel_info = panel_get_panel_info,
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
	.get_panel_initialized = get_panel_initialized,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
	.get_panel_factory_max_brightness = panel_get_factory_max_brightness,
	.get_panel_dynamic_fps =  panel_get_dynamic_fps,
	.panel_fod_lhbm_init = panel_fod_lhbm_init,
	.set_lhbm_fod = panel_set_lhbm_fod,
	.hbm_fod_control = panel_hbm_fod_control,
	.set_gray_by_temperature = panel_set_gray_by_temperature,
	.set_only_aod_backlight = panel_set_only_aod_backlight,
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

	struct drm_display_mode *mode_30, *mode_60, *mode_90, *mode_120;

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

	mode_60 = drm_mode_duplicate(connector->dev, &mode_60hz);
	if (!mode_60) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_60hz.hdisplay, mode_60hz.vdisplay,
			drm_mode_vrefresh(&mode_60hz));
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

	pr_info("%s n16u-42-02-0a-dsc +\n", __func__);

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

	panel_ctx = ctx;
	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE
			 | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}

	ctx->reset_gpio = devm_gpiod_get_index(dev, "reset", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ret = lcm_panel_vci_regulator_init(dev);
	if (!ret)
		lcm_panel_vci_enable(dev);
	else
		pr_err("%s init vibr regulator error\n", __func__);

	ctx->dvdd_gpio = devm_gpiod_get_index(dev, "dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(dev, "%s: cannot get dvdd-gpios %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	devm_gpiod_put(dev, ctx->dvdd_gpio);

	ctx->vddio18_gpio = devm_gpiod_get_index(dev, "vddio18", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddio18_gpio)) {
		dev_err(dev, "%s: cannot get vddio-gpios %ld\n",
			__func__, PTR_ERR(ctx->vddio18_gpio));
		return PTR_ERR(ctx->vddio18_gpio);
	}
	gpiod_set_value(ctx->vddio18_gpio, 1);
	devm_gpiod_put(dev, ctx->vddio18_gpio);

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->hbm_enabled = false;
	ctx->panel_info = panel_name;
	ctx->dynamic_fps = 60;
	ctx->panel_id = panel_id;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	ctx->factory_max_brightness = FACTORY_MAX_BRIGHTNESS;

	ext_params_60hz.err_flag_irq_gpio = of_get_named_gpio_flags(
		dev->of_node, "mi,esd-err-irq-gpio",
		0, (enum of_gpio_flags *)&(ext_params_60hz.err_flag_irq_flags));
	ext_params_30hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_30hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_90hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_90hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;
	ext_params_120hz.err_flag_irq_gpio = ext_params_60hz.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params_60hz.err_flag_irq_flags;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	mutex_init(&ctx->panel_lock);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	//mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params_60hz, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	this_panel = &ctx->panel;

	drm_register_fps_chg_callback(get_changed_fps);

	pr_info("%s n16u-42-02-0a-dsc-vdo -\n", __func__);
	return ret;
}

static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
#if defined(CONFIG_MTK_PANEL_EXT)
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
#endif
	drm_unregister_fps_chg_callback(get_changed_fps);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
#endif

	return 0;
}

static const struct of_device_id lcm_of_match[] = {
	{ .compatible = "n16u_42_02_0a_dsc_vdo,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "n16u_42_02_0a_dsc_vdo,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

module_param_string(oled_wp, oled_wp_cmdline, sizeof(oled_wp_cmdline), 0600);
MODULE_PARM_DESC(oled_wp, "oled_wp=<white_point_info>");

module_param_string(oled_lhbm, oled_lhbm_cmdline, sizeof(oled_lhbm_cmdline), 0600);
MODULE_PARM_DESC(oled_lhbm, "oled_lhbm=<local_hbm_info>");

MODULE_AUTHOR("ZhiHao Zhang <zhangzhihao6@xiaomi.com>");
MODULE_DESCRIPTION("n16u_42_02_0a_dsc_vdo oled panel driver");
MODULE_LICENSE("GPL v2");
