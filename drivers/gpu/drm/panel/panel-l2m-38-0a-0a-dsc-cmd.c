// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (C) 2021-2022 XiaoMi, Inc.
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
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#ifdef CONFIG_MI_DISP
#include <uapi/drm/mi_disp.h>
#endif

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#endif
#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "../mediatek/mediatek_v2/mi_disp/mi_dsi_panel.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_wqhd_l2m_l.h"
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_wqhd_l2m_r.h"
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_fhd_l2m_l.h"
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_fhd_l2m_r.h"
#endif
#include <asm/atomic.h>

#define DATA_RATE		1360
#define MODE1_HFP		32
#define MODE1_HSA		16
#define MODE1_HBP		32
#define MODE1_VFP		24
#define MODE1_VSA		8
#define MODE1_VBP		24
#define HACT_WQHD		1440
#define VACT_WQHD		3200
#define HACT_FHDP		1080
#define VACT_FHDP		2400

#define MAX_BRIGHTNESS_CLONE 	16383
#define MAX_NORMAL_BRIGHTNESS_51REG	2047
#define FPS_INIT_INDEX		31
#define FIXTE_FPS_INIT_INDEX	29
#define MODE_FPS_NUM		16
#define DOZE_HBM_DBV_LEVEL	229
#define DOZE_LBM_DBV_LEVEL	16
#define DSC_PANEL_NAME_L2M_SDC	0x6c026d01

static int fps_mode[MODE_FPS_NUM] = {60, 120, 90, 40, 30, 24, 10, 1, 120, 90, 60, 40, 30, 24, 10, 1};
static struct drm_display_mode mode_config[MODE_FPS_NUM];
static char bl_tb0[] = {0x51, 0x3, 0xff};
static const char *panel_name = "panel_name=dsi_l2m_38_0a_0a_dsc_cmd";
static atomic_t doze_enable = ATOMIC_INIT(0);
static atomic_t lhbm_enable = ATOMIC_INIT(0);
static int gDcThreshold = 450;
static bool gDcEnable = false;
static char oled_wp_cmdline[16] = {0};

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

//AMOLED 1.6V:DVDD_1P6,GPIO193
//AMOLED 1.8V:VDDI_1P8,GPIO194,VAUD18
//AMOLED 3V:VCI_3P0,VIBR

static struct regulator *disp_vci;
static struct regulator *disp_vddi;


static int lcm_panel_vci_regulator_init(struct device *dev)
{
	static int vibr_regulator_inited;
	int ret = 0;

	if (vibr_regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_vci = regulator_get(dev, "vibr");
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

static int lcm_panel_vddi_regulator_init(struct device *dev)
{
	static int vaud18_regulator_inited;
	int ret = 0;

	if (vaud18_regulator_inited)
               return ret;

	/* please only get regulator once in a driver */
	disp_vddi = regulator_get(dev, "vaud18");
	if (IS_ERR(disp_vddi)) { /* handle return value */
		ret = PTR_ERR(disp_vddi);
		pr_err("get disp_vddi fail, error: %d\n", ret);
		return ret;
	}

	vaud18_regulator_inited = 1;
	return ret; /* must be 0 */
}

static unsigned int vaud18_start_up = 1;
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
	pr_info("%s regulator_is_enabled = %d, vaud18_start_up = %d\n", __func__, status, vaud18_start_up);
	if (!status || vaud18_start_up){
		/* enable regulator */
		ret = regulator_enable(disp_vddi);
		if (ret < 0)
			pr_err("enable regulator disp_vddi fail, ret = %d\n", ret);
		vaud18_start_up = 0;
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

#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF

static struct LCM_setting_table init_setting_fhdp[] = {
	/* FHDP DSC setting */
	{0x9E, 128, {0x12, 0x00, 0x00, 0xAB, 0x30, 0x80, 0x09, 0x60,
			0x04, 0x38, 0x00, 0x20, 0x02, 0x1C, 0x02, 0x1C,
			0x02, 0x00, 0x02, 0x3B, 0x00, 0x20, 0x02, 0xD9,
			0x00, 0x07, 0x00, 0x0E, 0x03, 0x9D, 0x03, 0x2E,
			0x18, 0x00, 0x10, 0xF0, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A, 0x40,
			0x2A, 0xBE, 0x3A, 0xFC, 0x3A, 0xFA, 0x3A, 0xF8,
			0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6,
			0x4B, 0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x9D, 01, {0x01}},
	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {} },
	/* common setting */
	{0x35, 01, {0x00}},
	/* page address set */
	{0x2A, 04, {0x00, 0x00, 0x04, 0x37}},
	{0x2B, 04, {0x00, 0x00, 0x09, 0x5F}},
	/* scaler setting FHDP */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0D}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* FIX TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},
	{0xBD, 01, {0x21}},
	{0x60, 01, {0x01}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Dimming Setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0D, 0x63}},
	{0x63, 01, {0x04}},
	{0xB0, 03, {0x00, 0x0C, 0x63}},
	{0x63, 01, {0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x53, 01, {0x20}},
	{0x51, 02, {0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Err_flag setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x47, 0xF4}},
	{0xF4, 01, {0x01}},
	{0xED, 03, {0x47, 0x05, 0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* touch sync enable */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xF1, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x22, 0xB9}},
	{0xB9, 02, {0xB1, 0xA1}},
	{0xB0, 03, {0x00, 0x05, 0xF2}},
	{0xF2, 01, {0x52}},
	{0xF7, 01, {0x0F}},
	{0xF1, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0D}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* DIA setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x02, 0x67, 0x1D}},
	{0x1D, 01, {0x81}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* OPEC code */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x52, 0x1F}},
	{0x1F, 01, {0x02}},
	{0xB0, 03, {0x00, 0x53, 0x1F}},
	{0x1F, 94, {0x07, 0x01, 0x00, 0x02, 0xB9, 0x02, 0xBA, 0x07,
			0xA2, 0x07, 0xAA, 0x12, 0xC0, 0x12, 0xC1, 0x1A,
			0xBA, 0x1B, 0xC0, 0x1F, 0xE0, 0x00, 0x53, 0x00,
			0xC3, 0x03, 0x43, 0x07, 0xA0, 0x07, 0xA1, 0x07,
			0xA2, 0x07, 0xA6, 0x07, 0xA8, 0x07, 0xAA, 0x07,
			0xAC, 0x07, 0xAE, 0x07, 0xB0, 0x07, 0xB2, 0x07,
			0xB4, 0x07, 0xB6, 0x00, 0x03, 0x00, 0xA4, 0x00,
			0xC0, 0x00, 0xD1, 0x01, 0x86, 0x03, 0xFF, 0x07,
			0xFF, 0x30, 0x30, 0x35, 0x40, 0x35, 0x30, 0x40,
			0x40, 0x35, 0x80, 0x80, 0x80, 0x0A, 0x0F, 0x14,
			0x18, 0x1E, 0x3A, 0x78, 0x80, 0x80, 0x80, 0x80,
			0x80, 0x80, 0x80, 0x80, 0x01, 0xB1}},
	{0xB0, 03, {0x00, 0x78, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_90hz_fhdp[] = {
	/* FHDP DSC setting */
	{0x9E, 128, {0x12, 0x00, 0x00, 0xAB, 0x30, 0x80, 0x09, 0x60,
			0x04, 0x38, 0x00, 0x20, 0x02, 0x1C, 0x02, 0x1C,
			0x02, 0x00, 0x02, 0x3B, 0x00, 0x20, 0x02, 0xD9,
			0x00, 0x07, 0x00, 0x0E, 0x03, 0x9D, 0x03, 0x2E,
			0x18, 0x00, 0x10, 0xF0, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A, 0x40,
			0x2A, 0xBE, 0x3A, 0xFC, 0x3A, 0xFA, 0x3A, 0xF8,
			0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6,
			0x4B, 0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x9D, 01, {0x01}},
	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {} },
	/* common setting */
	{0x35, 01, {0x00}},
	/* page address set */
	{0x2A, 04, {0x00, 0x00, 0x04, 0x37}},
	{0x2B, 04, {0x00, 0x00, 0x09, 0x5F}},
	/* scaler setting FHDP */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0D}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* FIX TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 120Hz Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},
	{0xBD, 01, {0x21}},
	{0x60, 01, {0x00}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	/* 90 hz setting */
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0xA7}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xBD, 02, {0x21, 0x82}},
	{0x60, 01, {0x08}},   /* 0x08:90Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Dimming Setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0D, 0x63}},
	{0x63, 01, {0x04}},
	{0xB0, 03, {0x00, 0x0C, 0x63}},
	{0x63, 01, {0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x53, 01, {0x20}},
	{0x51, 02, {0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Err_flag setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x47, 0xF4}},
	{0xF4, 01, {0x01}},
	{0xED, 03, {0x47, 0x05, 0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* touch sync enable */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xF1, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x22, 0xB9}},
	{0xB9, 02, {0xB1, 0xA1}},
	{0xB0, 03, {0x00, 0x05, 0xF2}},
	{0xF2, 01, {0x52}},
	{0xF7, 01, {0x0F}},
	{0xF1, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0D}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* DIA setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x02, 0x67, 0x1D}},
	{0x1D, 01, {0x81}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* OPEC code */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x52, 0x1F}},
	{0x1F, 01, {0x02}},
	{0xB0, 03, {0x00, 0x53, 0x1F}},
	{0x1F, 94, {0x07, 0x01, 0x00, 0x02, 0xB9, 0x02, 0xBA, 0x07,
			0xA2, 0x07, 0xAA, 0x12, 0xC0, 0x12, 0xC1, 0x1A,
			0xBA, 0x1B, 0xC0, 0x1F, 0xE0, 0x00, 0x53, 0x00,
			0xC3, 0x03, 0x43, 0x07, 0xA0, 0x07, 0xA1, 0x07,
			0xA2, 0x07, 0xA6, 0x07, 0xA8, 0x07, 0xAA, 0x07,
			0xAC, 0x07, 0xAE, 0x07, 0xB0, 0x07, 0xB2, 0x07,
			0xB4, 0x07, 0xB6, 0x00, 0x03, 0x00, 0xA4, 0x00,
			0xC0, 0x00, 0xD1, 0x01, 0x86, 0x03, 0xFF, 0x07,
			0xFF, 0x30, 0x30, 0x35, 0x40, 0x35, 0x30, 0x40,
			0x40, 0x35, 0x80, 0x80, 0x80, 0x0A, 0x0F, 0x14,
			0x18, 0x1E, 0x3A, 0x78, 0x80, 0x80, 0x80, 0x80,
			0x80, 0x80, 0x80, 0x80, 0x01, 0xB1}},
	{0xB0, 03, {0x00, 0x78, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_fixte_fhdp[] = {
	/* FHDP DSC setting */
	{0x9E, 128, {0x12, 0x00, 0x00, 0xAB, 0x30, 0x80, 0x09, 0x60,
			0x04, 0x38, 0x00, 0x20, 0x02, 0x1C, 0x02, 0x1C,
			0x02, 0x00, 0x02, 0x3B, 0x00, 0x20, 0x02, 0xD9,
			0x00, 0x07, 0x00, 0x0E, 0x03, 0x9D, 0x03, 0x2E,
			0x18, 0x00, 0x10, 0xF0, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A, 0x40,
			0x2A, 0xBE, 0x3A, 0xFC, 0x3A, 0xFA, 0x3A, 0xF8,
			0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6,
			0x4B, 0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x9D, 01, {0x01}},
	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {} },
	/* common setting */
	{0x35, 01, {0x00}},
	/* page address set */
	{0x2A, 04, {0x00, 0x00, 0x04, 0x37}},
	{0x2B, 04, {0x00, 0x00, 0x09, 0x5F}},
	/* scaler setting FHDP */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0D}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* FIXED TE ON */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x41}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x0C, 0x9E, 0x0C, 0x9E, 0x00, 0x1D}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 10Hz Setting(force increasing on) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x23, 0x02}},    /* Auto mode On */
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x02}},
	{0xB0, 03, {0x00, 0x14, 0xBD}},
	{0xBD, 01, {0x0B}},      /* 0B:10hz, 77:1hz */
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x03, 0x00}},    /* Control in Auto mode */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},    /* 3 sync */
	/* Dimming Setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0D, 0x63}},
	{0x63, 01, {0x04}},
	{0xB0, 03, {0x00, 0x0C, 0x63}},
	{0x63, 01, {0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x53, 01, {0x20}},
	{0x51, 02, {0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Err_flag setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x47, 0xF4}},
	{0xF4, 01, {0x01}},
	{0xED, 03, {0x47, 0x05, 0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* touch sync enable */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xF1, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x22, 0xB9}},
	{0xB9, 02, {0xB1, 0xA1}},
	{0xB0, 03, {0x00, 0x05, 0xF2}},
	{0xF2, 01, {0x52}},
	{0xF7, 01, {0x0F}},
	{0xF1, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0D}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* DIA setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x02, 0x67, 0x1D}},
	{0x1D, 01, {0x81}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* OPEC code */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x52, 0x1F}},
	{0x1F, 01, {0x02}},
	{0xB0, 03, {0x00, 0x53, 0x1F}},
	{0x1F, 94, {0x07, 0x01, 0x00, 0x02, 0xB9, 0x02, 0xBA, 0x07,
			0xA2, 0x07, 0xAA, 0x12, 0xC0, 0x12, 0xC1, 0x1A,
			0xBA, 0x1B, 0xC0, 0x1F, 0xE0, 0x00, 0x53, 0x00,
			0xC3, 0x03, 0x43, 0x07, 0xA0, 0x07, 0xA1, 0x07,
			0xA2, 0x07, 0xA6, 0x07, 0xA8, 0x07, 0xAA, 0x07,
			0xAC, 0x07, 0xAE, 0x07, 0xB0, 0x07, 0xB2, 0x07,
			0xB4, 0x07, 0xB6, 0x00, 0x03, 0x00, 0xA4, 0x00,
			0xC0, 0x00, 0xD1, 0x01, 0x86, 0x03, 0xFF, 0x07,
			0xFF, 0x30, 0x30, 0x35, 0x40, 0x35, 0x30, 0x40,
			0x40, 0x35, 0x80, 0x80, 0x80, 0x0A, 0x0F, 0x14,
			0x18, 0x1E, 0x3A, 0x78, 0x80, 0x80, 0x80, 0x80,
			0x80, 0x80, 0x80, 0x80, 0x01, 0xB1}},
	{0xB0, 03, {0x00, 0x78, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_wqhd[] = {
	/* WQHD DSC setting */
	{0x9E, 128, {0x12, 0x00, 0x00, 0xAB, 0x30, 0x80, 0x0C, 0x80,
			0x05, 0xA0, 0x00, 0x19, 0x02, 0xD0, 0x02, 0xD0,
			0x02, 0x00, 0x02, 0x86, 0x00, 0x20, 0x02, 0x9E,
			0x00, 0x0A, 0x00, 0x0D, 0x04, 0x56, 0x03, 0x0D,
			0x18, 0x00, 0x10, 0xF0, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A, 0x40,
			0x2A, 0xBE, 0x3A, 0xFC, 0x3A, 0xFA, 0x3A, 0xF8,
			0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6,
			0x4B, 0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x9D, 01, {0x01}},
	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {} },
	/* common setting */
	{0x35, 01, {0x00}},
	/* page address set */
	{0x2A, 04, {0x00, 0x00, 0x05, 0x9F}},
	{0x2B, 04, {0x00, 0x00, 0x0C, 0x7F}},
	/* FIXED TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* scaler setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0C}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},
	{0xBD, 01, {0x21}},
	{0x60, 01, {0x01}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Dimming Setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0D, 0x63}},
	{0x63, 01, {0x04}},
	{0xB0, 03, {0x00, 0x0C, 0x63}},
	{0x63, 01, {0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x53, 01, {0x20}},
	{0x51, 02, {0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Err_flag setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x47, 0xF4}},
	{0xF4, 01, {0x01}},
	{0xED, 03, {0x47, 0x05, 0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* touch sync enable */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xF1, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x22, 0xB9}},
	{0xB9, 02, {0xB1, 0xA1}},
	{0xB0, 03, {0x00, 0x05, 0xF2}},
	{0xF2, 01, {0x52}},
	{0xF7, 01, {0x0F}},
	{0xF1, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0C}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* DIA setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x02, 0x67, 0x1D}},
	{0x1D, 01, {0x81}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* OPEC code */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x52, 0x1F}},
	{0x1F, 01, {0x02}},
	{0xB0, 03, {0x00, 0x53, 0x1F}},
	{0x1F, 94, {0x07, 0x01, 0x00, 0x02, 0xB9, 0x02, 0xBA, 0x07,
			0xA2, 0x07, 0xAA, 0x12, 0xC0, 0x12, 0xC1, 0x1A,
			0xBA, 0x1B, 0xC0, 0x1F, 0xE0, 0x00, 0x53, 0x00,
			0xC3, 0x03, 0x43, 0x07, 0xA0, 0x07, 0xA1, 0x07,
			0xA2, 0x07, 0xA6, 0x07, 0xA8, 0x07, 0xAA, 0x07,
			0xAC, 0x07, 0xAE, 0x07, 0xB0, 0x07, 0xB2, 0x07,
			0xB4, 0x07, 0xB6, 0x00, 0x03, 0x00, 0xA4, 0x00,
			0xC0, 0x00, 0xD1, 0x01, 0x86, 0x03, 0xFF, 0x07,
			0xFF, 0x30, 0x30, 0x35, 0x40, 0x35, 0x30, 0x40,
			0x40, 0x35, 0x80, 0x80, 0x80, 0x0A, 0x0F, 0x14,
			0x18, 0x1E, 0x3A, 0x78, 0x80, 0x80, 0x80, 0x80,
			0x80, 0x80, 0x80, 0x80, 0x01, 0xB1}},
	{0xB0, 03, {0x00, 0x78, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_90hz_wqhd[] = {
	/* WQHD DSC setting */
	{0x9E, 128, {0x12, 0x00, 0x00, 0xAB, 0x30, 0x80, 0x0C, 0x80,
			0x05, 0xA0, 0x00, 0x19, 0x02, 0xD0, 0x02, 0xD0,
			0x02, 0x00, 0x02, 0x86, 0x00, 0x20, 0x02, 0x9E,
			0x00, 0x0A, 0x00, 0x0D, 0x04, 0x56, 0x03, 0x0D,
			0x18, 0x00, 0x10, 0xF0, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A, 0x40,
			0x2A, 0xBE, 0x3A, 0xFC, 0x3A, 0xFA, 0x3A, 0xF8,
			0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6,
			0x4B, 0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x9D, 01, {0x01}},
	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {} },
	/* common setting */
	{0x35, 01, {0x00}},
	/* page address set */
	{0x2A, 04, {0x00, 0x00, 0x05, 0x9F}},
	{0x2B, 04, {0x00, 0x00, 0x0C, 0x7F}},
	/* FIXED TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* scaler setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0C}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 120hz Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},
	{0xBD, 01, {0x21}},
	{0x60, 01, {0x00}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	/* 90 hz setting */
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0xA7}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xBD, 02, {0x21, 0x82}},
	{0x60, 01, {0x08}},   /* 0x08:90Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Dimming Setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0D, 0x63}},
	{0x63, 01, {0x04}},
	{0xB0, 03, {0x00, 0x0C, 0x63}},
	{0x63, 01, {0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x53, 01, {0x20}},
	{0x51, 02, {0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Err_flag setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x47, 0xF4}},
	{0xF4, 01, {0x01}},
	{0xED, 03, {0x47, 0x05, 0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* touch sync enable */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xF1, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x22, 0xB9}},
	{0xB9, 02, {0xB1, 0xA1}},
	{0xB0, 03, {0x00, 0x05, 0xF2}},
	{0xF2, 01, {0x52}},
	{0xF7, 01, {0x0F}},
	{0xF1, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0C}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* DIA setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x02, 0x67, 0x1D}},
	{0x1D, 01, {0x81}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* OPEC code */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x52, 0x1F}},
	{0x1F, 01, {0x02}},
	{0xB0, 03, {0x00, 0x53, 0x1F}},
	{0x1F, 94, {0x07, 0x01, 0x00, 0x02, 0xB9, 0x02, 0xBA, 0x07,
			0xA2, 0x07, 0xAA, 0x12, 0xC0, 0x12, 0xC1, 0x1A,
			0xBA, 0x1B, 0xC0, 0x1F, 0xE0, 0x00, 0x53, 0x00,
			0xC3, 0x03, 0x43, 0x07, 0xA0, 0x07, 0xA1, 0x07,
			0xA2, 0x07, 0xA6, 0x07, 0xA8, 0x07, 0xAA, 0x07,
			0xAC, 0x07, 0xAE, 0x07, 0xB0, 0x07, 0xB2, 0x07,
			0xB4, 0x07, 0xB6, 0x00, 0x03, 0x00, 0xA4, 0x00,
			0xC0, 0x00, 0xD1, 0x01, 0x86, 0x03, 0xFF, 0x07,
			0xFF, 0x30, 0x30, 0x35, 0x40, 0x35, 0x30, 0x40,
			0x40, 0x35, 0x80, 0x80, 0x80, 0x0A, 0x0F, 0x14,
			0x18, 0x1E, 0x3A, 0x78, 0x80, 0x80, 0x80, 0x80,
			0x80, 0x80, 0x80, 0x80, 0x01, 0xB1}},
	{0xB0, 03, {0x00, 0x78, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_fixte_wqhd[] = {
	/* WQHD DSC setting */
	{0x9E, 128, {0x12, 0x00, 0x00, 0xAB, 0x30, 0x80, 0x0C, 0x80,
			0x05, 0xA0, 0x00, 0x19, 0x02, 0xD0, 0x02, 0xD0,
			0x02, 0x00, 0x02, 0x86, 0x00, 0x20, 0x02, 0x9E,
			0x00, 0x0A, 0x00, 0x0D, 0x04, 0x56, 0x03, 0x0D,
			0x18, 0x00, 0x10, 0xF0, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A, 0x40,
			0x2A, 0xBE, 0x3A, 0xFC, 0x3A, 0xFA, 0x3A, 0xF8,
			0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6,
			0x4B, 0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x9D, 01, {0x01}},
	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {} },
	/* common setting */
	{0x35, 01, {0x00}},
	/* page address set */
	{0x2A, 04, {0x00, 0x00, 0x05, 0x9F}},
	{0x2B, 04, {0x00, 0x00, 0x0C, 0x7F}},
	/* scaler setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0C}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* FIXED TE ON */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x41}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x0C, 0x9E, 0x0C, 0x9E, 0x00, 0x1D}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 10Hz Setting(force increasing on) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x23, 0x02}},    /* Auto mode On */
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x02}},
	{0xB0, 03, {0x00, 0x14, 0xBD}},
	{0xBD, 01, {0x0B}},      /* 0B:10hz, 77:1hz */
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x03, 0x00}},    /* Control in Auto mode */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},    /* 3 sync */
	/* Dimming Setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0D, 0x63}},
	{0x63, 01, {0x04}},
	{0xB0, 03, {0x00, 0x0C, 0x63}},
	{0x63, 01, {0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x53, 01, {0x20}},
	{0x51, 02, {0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* Err_flag setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x47, 0xF4}},
	{0xF4, 01, {0x01}},
	{0xED, 03, {0x47, 0x05, 0x20}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* touch sync enable */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xF1, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x22, 0xB9}},
	{0xB9, 02, {0xB1, 0xA1}},
	{0xB0, 03, {0x00, 0x05, 0xF2}},
	{0xF2, 01, {0x52}},
	{0xF7, 01, {0x0F}},
	{0xF1, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0C}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* DIA setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x02, 0x67, 0x1D}},
	{0x1D, 01, {0x81}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* OPEC code */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x52, 0x1F}},
	{0x1F, 01, {0x02}},
	{0xB0, 03, {0x00, 0x53, 0x1F}},
	{0x1F, 94, {0x07, 0x01, 0x00, 0x02, 0xB9, 0x02, 0xBA, 0x07,
			0xA2, 0x07, 0xAA, 0x12, 0xC0, 0x12, 0xC1, 0x1A,
			0xBA, 0x1B, 0xC0, 0x1F, 0xE0, 0x00, 0x53, 0x00,
			0xC3, 0x03, 0x43, 0x07, 0xA0, 0x07, 0xA1, 0x07,
			0xA2, 0x07, 0xA6, 0x07, 0xA8, 0x07, 0xAA, 0x07,
			0xAC, 0x07, 0xAE, 0x07, 0xB0, 0x07, 0xB2, 0x07,
			0xB4, 0x07, 0xB6, 0x00, 0x03, 0x00, 0xA4, 0x00,
			0xC0, 0x00, 0xD1, 0x01, 0x86, 0x03, 0xFF, 0x07,
			0xFF, 0x30, 0x30, 0x35, 0x40, 0x35, 0x30, 0x40,
			0x40, 0x35, 0x80, 0x80, 0x80, 0x0A, 0x0F, 0x14,
			0x18, 0x1E, 0x3A, 0x78, 0x80, 0x80, 0x80, 0x80,
			0x80, 0x80, 0x80, 0x80, 0x01, 0xB1}},
	{0xB0, 03, {0x00, 0x78, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{0x29, 01, {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_gir_on[] = {
	{0xF0, 0x02, {0x5A, 0x5A}},
	{0xB0, 0x03, {0x02, 0xB5, 0x1D}},
	{0x1D, 0x28, {0x27, 0x23, 0x6C, 0x03, 0x4E, 0x86, 0x01, 0xFF,
			0x10, 0x73, 0xFF, 0x10, 0xFF, 0x6B, 0x8C, 0x2D,
			0x06, 0x07, 0x06, 0x1B, 0x1F, 0x18, 0x24, 0x29,
			0x20, 0x2B, 0x31, 0x26, 0x2E, 0x34, 0x28, 0xA4,
			0xE4, 0xA4, 0x08, 0x74, 0x80, 0x00, 0x00, 0x22}},
	{0xF0, 0x02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_gir_off[] = {
	{0xF0, 0x02, {0x5A, 0x5A}},
	{0xB0, 0x03, {0x02, 0xB5, 0x1D}},
	{0x1D, 0x28, {0x27, 0x03, 0xA4, 0x03, 0x5A, 0x80, 0x01, 0xFF,
			0x10, 0x73, 0xFF, 0x10, 0xFF, 0x6B, 0x8C, 0x2D,
			0x07, 0x07, 0x07, 0x1E, 0x1E, 0x1E, 0x28, 0x28,
			0x28, 0x2F, 0x2F, 0x2F, 0x32, 0x32, 0x32, 0xA4,
			0xE4, 0xA4, 0x08, 0x74, 0x80, 0x00, 0x00, 0x22}},
	{0xF0, 0x02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
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

	pr_info("%s\n", __func__);
	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(50);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(150);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->crc_level = 0;

	atomic_set(&doze_enable, 0);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

	lcm_panel_vddi_enable(ctx->dev);
	udelay(5 * 1000);

	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
		"dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);
	udelay(5 * 1000);

	lcm_panel_vci_enable(ctx->dev);

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return -1;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(10 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	//lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif
#ifdef PANEL_SUPPORT_READBACK
	lcm_panel_get_data(ctx);
#endif

	return ret;
}

static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s: +\n", __func__);

	if (ctx->enabled) {
		pr_info("%s: panel has been enabled, nothing to do!\n", __func__);
		goto err;
	}

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;
err:
	pr_info("%s: -\n", __func__);
	return 0;
}

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 1562,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 646,
		.scale_value = 32,
		.increment_interval = 670,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1110,
		.slice_bpg_offset = 781,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_WQHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_WQHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_wqhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_wqhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_wqhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_wqhd_r,
#endif
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 90,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 646,
		.scale_value = 32,
		.increment_interval = 670,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1110,
		.slice_bpg_offset = 781,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_WQHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_WQHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_wqhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_wqhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_wqhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_wqhd_r,
#endif
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 800,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 646,
		.scale_value = 32,
		.increment_interval = 670,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1110,
		.slice_bpg_offset = 781,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_WQHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_WQHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_wqhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_wqhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_wqhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_wqhd_r,
#endif
};

static struct mtk_panel_params ext_params_40hz = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 1300,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 646,
		.scale_value = 32,
		.increment_interval = 670,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1110,
		.slice_bpg_offset = 781,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_WQHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_WQHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_wqhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_wqhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_wqhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_wqhd_r,
#endif
};

static struct mtk_panel_params ext_params_30hz = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 1300,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 646,
		.scale_value = 32,
		.increment_interval = 670,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1110,
		.slice_bpg_offset = 781,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_WQHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_WQHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_wqhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_wqhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_wqhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_wqhd_r,
#endif
};

static struct mtk_panel_params ext_params_24hz = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 1300,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 646,
		.scale_value = 32,
		.increment_interval = 670,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1110,
		.slice_bpg_offset = 781,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_WQHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_WQHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_wqhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_wqhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_wqhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_wqhd_r,
#endif
};

static struct mtk_panel_params ext_params_10hz = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 90,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 646,
		.scale_value = 32,
		.increment_interval = 670,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1110,
		.slice_bpg_offset = 781,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_WQHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_WQHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_wqhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_wqhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_wqhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_wqhd_r,
#endif
};

static struct mtk_panel_params ext_params_1hz = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 90,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 646,
		.scale_value = 32,
		.increment_interval = 670,
		.decrement_interval = 10,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 1110,
		.slice_bpg_offset = 781,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_WQHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_WQHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_wqhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_wqhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_wqhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_wqhd_r,
#endif
};

static struct mtk_panel_params ext_params_fhdp = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 1562,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 571,
		.scale_value = 32,
		.increment_interval = 729,
		.decrement_interval = 7,
		.line_bpg_offset = 14,
		.nfl_bpg_offset = 925,
		.slice_bpg_offset = 814,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_FHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_FHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_fhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_fhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_fhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_fhd_r,
#endif
};

static struct mtk_panel_params ext_params_120hz_fhdp = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 962,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 571,
		.scale_value = 32,
		.increment_interval = 729,
		.decrement_interval = 7,
		.line_bpg_offset = 14,
		.nfl_bpg_offset = 925,
		.slice_bpg_offset = 814,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_FHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_FHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_fhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_fhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_fhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_fhd_r,
#endif
};

static struct mtk_panel_params ext_params_90hz_fhdp = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 962,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 571,
		.scale_value = 32,
		.increment_interval = 729,
		.decrement_interval = 7,
		.line_bpg_offset = 14,
		.nfl_bpg_offset = 925,
		.slice_bpg_offset = 814,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_FHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_FHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_fhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_fhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_fhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_fhd_r,
#endif
};

static struct mtk_panel_params ext_params_40hz_fhdp = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 1562,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 571,
		.scale_value = 32,
		.increment_interval = 729,
		.decrement_interval = 7,
		.line_bpg_offset = 14,
		.nfl_bpg_offset = 925,
		.slice_bpg_offset = 814,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_FHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_FHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_fhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_fhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_fhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_fhd_r,
#endif
};

static struct mtk_panel_params ext_params_30hz_fhdp = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 1562,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 571,
		.scale_value = 32,
		.increment_interval = 729,
		.decrement_interval = 7,
		.line_bpg_offset = 14,
		.nfl_bpg_offset = 925,
		.slice_bpg_offset = 814,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_FHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_FHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_fhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_fhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_fhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_fhd_r,
#endif
};

static struct mtk_panel_params ext_params_24hz_fhdp = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 1562,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 571,
		.scale_value = 32,
		.increment_interval = 729,
		.decrement_interval = 7,
		.line_bpg_offset = 14,
		.nfl_bpg_offset = 925,
		.slice_bpg_offset = 814,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_FHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_FHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_fhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_fhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_fhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_fhd_r,
#endif
};

static struct mtk_panel_params ext_params_10hz_fhdp = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 962,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 571,
		.scale_value = 32,
		.increment_interval = 729,
		.decrement_interval = 7,
		.line_bpg_offset = 14,
		.nfl_bpg_offset = 925,
		.slice_bpg_offset = 814,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_FHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_FHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_fhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_fhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_fhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_fhd_r,
#endif
};

static struct mtk_panel_params ext_params_1hz_fhdp = {
	.pll_clk = 680,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 70128,
	.physical_height_um = 155840,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 962,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 18,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2400,
		.pic_width = 1080,
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 571,
		.scale_value = 32,
		.increment_interval = 729,
		.decrement_interval = 7,
		.line_bpg_offset = 14,
		.nfl_bpg_offset = 925,
		.slice_bpg_offset = 814,
		.initial_offset = 6144,
		.final_offset = 4336,
		.flatness_minqp = 7,
		.flatness_maxqp = 16,
		.rc_model_size = 8192,
		.rc_edge_factor = 6,
		.rc_quant_incr_limit0 = 15,
		.rc_quant_incr_limit1 = 15,
		.rc_tgt_offset_hi = 3,
		.rc_tgt_offset_lo = 3,
		.dsc_config_panel_name = DSC_PANEL_NAME_L2M_SDC,
		},
	.data_rate = DATA_RATE,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP_FHD,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT_FHD,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_fhd_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_fhd_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_fhd_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_fhd_r,
#endif
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

static int mtk_panel_ext_param_get(struct drm_panel *panel,
	struct drm_connector *connector,
	struct mtk_panel_params **ext_param,
	unsigned int mode)
{
	int ret = 0;

	if (mode == 0)
		*ext_param = &ext_params;
	else if (mode == 1)
		*ext_param = &ext_params_120hz;
	else if (mode == 2)
		*ext_param = &ext_params_90hz;
	else if (mode == 3)
		*ext_param = &ext_params_40hz;
	else if (mode == 4)
		*ext_param = &ext_params_30hz;
	else if (mode == 5)
		*ext_param = &ext_params_24hz;
	else if (mode == 6)
		*ext_param = &ext_params_10hz;
	else if (mode == 7)
		*ext_param = &ext_params_1hz;
	else if (mode == 8)
		*ext_param = &ext_params_120hz_fhdp;
	else if (mode == 9)
		*ext_param = &ext_params_90hz_fhdp;
	else if (mode == 10)
		*ext_param = &ext_params_fhdp;
	else if (mode == 11)
		*ext_param = &ext_params_40hz_fhdp;
	else if (mode == 12)
		*ext_param = &ext_params_30hz_fhdp;
	else if (mode == 13)
		*ext_param = &ext_params_24hz_fhdp;
	else if (mode == 14)
		*ext_param = &ext_params_10hz_fhdp;
	else if (mode == 15)
		*ext_param = &ext_params_1hz_fhdp;
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
	pr_info("%s thh drm_mode_vrefresh = %d, m->hdisplay = %d\n",
		__func__, drm_mode_vrefresh(m), m->hdisplay);

	if (drm_mode_vrefresh(m) == fps_mode[0] && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == fps_mode[1] && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == fps_mode[2] && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m) == fps_mode[3] && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params_40hz;
	else if (drm_mode_vrefresh(m) == fps_mode[4] && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params_30hz;
	else if (drm_mode_vrefresh(m) == fps_mode[5] && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params_24hz;
	else if (drm_mode_vrefresh(m) == fps_mode[6] && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params_10hz;
	else if (drm_mode_vrefresh(m) == fps_mode[7] && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params_1hz;
	else if (drm_mode_vrefresh(m) == fps_mode[8] && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_120hz_fhdp;
	else if (drm_mode_vrefresh(m) == fps_mode[9] && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_90hz_fhdp;
	else if (drm_mode_vrefresh(m) == fps_mode[10] && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_fhdp;
	else if (drm_mode_vrefresh(m) == fps_mode[11] && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_40hz_fhdp;
	else if (drm_mode_vrefresh(m) == fps_mode[12] && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_30hz_fhdp;
	else if (drm_mode_vrefresh(m) == fps_mode[13] && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_24hz_fhdp;
	else if (drm_mode_vrefresh(m) == fps_mode[14] && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_10hz_fhdp;
	else if (drm_mode_vrefresh(m) == fps_mode[15] && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_1hz_fhdp;
	else
		ret = 1;

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
	char bl_tb[] = {0x51, 0x07, 0xff};
	char hbm_on_write_cmd0[] = {0x53, 0xE8};
	char hbm_off_write_cmd0[] = {0x53, 0x28};

	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;
	struct drm_panel *panel = mtk_dsi->panel;
	struct lcm *ctx;

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return -1;
	}
	ctx = panel_to_lcm(panel);

	if (gDcEnable && level && level < gDcThreshold)
		level = gDcThreshold;

	if (level > MAX_NORMAL_BRIGHTNESS_51REG) {
		level -= (MAX_NORMAL_BRIGHTNESS_51REG+1);
		if (!ctx->hbm_en) {
			cb(dsi, handle, hbm_on_write_cmd0, ARRAY_SIZE(hbm_on_write_cmd0));
			ctx->hbm_en = true;
		}
	} else if (ctx->hbm_en) {
		cb(dsi, handle, hbm_off_write_cmd0, ARRAY_SIZE(hbm_off_write_cmd0));
		ctx->hbm_en = false;
	}

	mtk_dsi->mi_cfg.last_bl_level = level;
	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
		mtk_dsi->mi_cfg.last_no_zero_bl_level = level;
	}

	bl_tb[1] = (level >> 8) & 0xFF;
	bl_tb[2] = level & 0xFF;
	if (!cb)
		return -1;
	if (atomic_read(&doze_enable)) {
		pr_info("%s: Return it when aod on, %d %d %d!\n", __func__, level, bl_tb[1], bl_tb[2]);
		return 0;
	}
	pr_info("%s %d %d %d\n", __func__, level, bl_tb[1], bl_tb[2]);
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
	return 0;
}

static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level)
{
	int ret = 0;
	char brightness_cmd[] = {0x51, 0x00, 0x00};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 1,
	};

	if (!panel) {
		pr_err(": panel is NULL\n", __func__);
		return -1;
	}

	if (level) {
		brightness_cmd[1] = (level >> 8) & 0xFF;
		brightness_cmd[2] = level & 0xFF;
	}

	cmd_msg.type[0] = ARRAY_SIZE(brightness_cmd) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = brightness_cmd;
	cmd_msg.tx_len[0] = ARRAY_SIZE(brightness_cmd);

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
	return ret;
}

static struct LCM_setting_table mode_60hz_setting_exit_90hz[] = {
	/* frequency select 60hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0x5A, 0x5A}},  /* 1 sync */
	{REGFLAG_DELAY, 5, {} },
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},  /* 2 sync */
	{REGFLAG_DELAY, 5, {} },
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x01}},     /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},     /* 2 sync */
	{0xF0, 02, {0xA5, 0xA5}},  /* 2 sync */
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_120hz_setting_exit_90hz[] = {
	/* frequency select 120hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x00}},
	{0xF7, 01, {0x0F}},
	{REGFLAG_DELAY, 5, {} },
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},  /* 2 sync */
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_40hz_setting_exit_90hz[] = {
	/* frequency select 40hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0x5A, 0x5A}},  /* 1 sync */
	{REGFLAG_DELAY, 5, {} },
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},  /* 2 sync */
	{REGFLAG_DELAY, 5, {} },
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x02}},     /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},     /* 2 sync */
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_30hz_setting_exit_90hz[] = {
	/* frequency select 30hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0x5A, 0x5A}},  /* 1 sync */
	{REGFLAG_DELAY, 5, {} },
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},  /* 2 sync */
	{REGFLAG_DELAY, 5, {} },
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x03}},     /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},     /* 2 sync */
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_24hz_setting_exit_90hz[] = {
	/* frequency select 24hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0x5A, 0x5A}},  /* 1 sync */
	{REGFLAG_DELAY, 5, {} },
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},  /* 2 sync */
	{REGFLAG_DELAY, 5, {} },
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x04}},     /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},     /* 2 sync */
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_10hz_setting_exit_90hz[] = {
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x00}},     /* 120hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},  /* 1 sync */
	{REGFLAG_DELAY, 5, {} },
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},  /* 2 sync */
	{REGFLAG_DELAY, 5, {} },
	/* FIX TE ON */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x41}},    /* FIXED TE ON*/
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x0C, 0x9E, 0x0C, 0x9E, 0x00, 0x1D}}, /* FIXED TE ON*/
	{0xF7, 02, {0x0F}},      /* LTPS UPDATE */
	{0xF0, 02, {0xA5, 0xA5}},
	/* 10Hz Setting(force increasing on) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x23, 0x02}},    /* Auto mode On */
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x02}},
	{0xB0, 03, {0x00, 0x14, 0xBD}},
	{0xBD, 01, {0x0B}},      /* 0B:10hz */
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x03, 0x00}},    /* Control in Auto mode */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},    /* 3 sync */
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_1hz_setting_exit_90hz[] = {
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x00}},     /* 120hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},  /* 1 sync */
	{REGFLAG_DELAY, 5, {} },
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},  /* 2 sync */
	{REGFLAG_DELAY, 5, {} },
	/* FIX TE ON */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x41}},    /* FIXED TE ON*/
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x0C, 0x9E, 0x0C, 0x9E, 0x00, 0x1D}}, /* FIXED TE ON*/
	{0xF7, 02, {0x0F}},      /* LTPS UPDATE */
	{0xF0, 02, {0xA5, 0xA5}},
	/* 10Hz Setting(force increasing on) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x23, 0x02}},    /* Auto mode On */
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x02}},
	{0xB0, 03, {0x00, 0x14, 0xBD}},
	{0xBD, 01, {0x77}},      /* 0B:10hz, 77:1hz */
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x03, 0x00}},    /* Control in Auto mode */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},    /* 3 sync */
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_60hz_setting[] = {
	/* FIX TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 60Hz Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},    /* Step delay */
	{0xBD, 01, {0x21}},      /* Manual mode */
	{0x60, 01, {0x01}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_120hz_setting[] = {
	/* FIX TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 60Hz Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},    /* Step delay */
	{0xBD, 01, {0x21}},      /* Manual mode */
	{0x60, 01, {0x00}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_90hz_setting[] = {
	/* FIX TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 120Hz Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},    /* Step delay */
	{0xBD, 01, {0x21}},      /* Manual mode */
	{0x60, 01, {0x00}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},
	{REGFLAG_DELAY, 5, {} },
	/* 90 hz setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0xA7}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xBD, 02, {0x21, 0x82}},
	{0x60, 01, {0x08}},      /* 0x08:90Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},    /* 2 sync */
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_90hz_front_setting[] = {
	/* FIX TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 120Hz Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},    /* Step delay */
	{0xBD, 01, {0x21}},      /* Manual mode */
	{0x60, 01, {0x00}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_90hz_back_setting[] = {
	/* 90 hz setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0xA7}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xBD, 02, {0x21, 0x82}},
	{0x60, 01, {0x08}},      /* 0x08:90Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},    /* 2 sync */
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_40hz_setting[] = {
	/* FIX TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 60Hz Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},    /* Step delay */
	{0xBD, 01, {0x21}},      /* Manual mode */
	{0x60, 01, {0x02}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_30hz_setting[] = {
	/* FIX TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 60Hz Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},    /* Step delay */
	{0xBD, 01, {0x21}},      /* Manual mode */
	{0x60, 01, {0x03}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_24hz_setting[] = {
	/* FIX TE OFF */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 24Hz Setting(force increasing off) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x21, 0x82}},
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x00}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x01}},
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x00, 0x00}},    /* Step delay */
	{0xBD, 01, {0x21}},      /* Manual mode */
	{0x60, 01, {0x04}},      /* 0x00:120Hz, 0x01:60Hz, 0x03:30Hz, 0x07:10Hz */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_10hz_setting[] = {
	/* FIX TE ON */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x41}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x0C, 0x9E, 0x0C, 0x9E, 0x00, 0x1D}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 10Hz Setting(force increasing on) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x23, 0x02}},    /* Auto mode On */
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x02}},
	{0xB0, 03, {0x00, 0x14, 0xBD}},
	{0xBD, 01, {0x0B}},      /* 0B:10hz */
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x03, 0x00}},    /* Control in Auto mode */
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_1hz_setting[] = {
	/* FIX TE ON */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x41}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x0C, 0x9E, 0x0C, 0x9E, 0x00, 0x1D}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* 10Hz Setting(force increasing on) */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x08, 0xCB}},
	{0xCB, 01, {0x27}},
	{0xBD, 02, {0x23, 0x02}},    /* Auto mode On */
	{0xB0, 03, {0x00, 0x10, 0xBD}},
	{0xBD, 01, {0x10}},
	{0xB0, 03, {0x00, 0x21, 0xBD}},
	{0xBD, 14, {0x03, 0x00, 0x06, 0x00, 0x09, 0x00, 0x0C, 0x00, 0x0F, 0x00, 0x15, 0x00, 0x21, 0x00}},
	{0xB0, 03, {0x00, 0x61, 0xBD}},
	{0xBD, 9,  {0x04, 0x00, 0x08, 0x00, 0x0C, 0x00, 0x10, 0x00, 0x74}},
	{0xB0, 03, {0x00, 0x12, 0xBD}},
	{0xBD, 01, {0x02}},
	{0xB0, 03, {0x00, 0x14, 0xBD}},
	{0xBD, 01, {0x77}},      /* 0B:10hz */
	{0xB0, 03, {0x00, 0x16, 0xBD}},
	{0xBD, 02, {0x03, 0x00}},    /* Control in Auto mode */
	{0xF7, 01, {0x0F}},//	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_wqhd_setting[] = {
	/* WQHD DSC setting */
	{0x9E, 128, {0x12, 0x00, 0x00, 0xAB, 0x30, 0x80, 0x0C, 0x80,
			0x05, 0xA0, 0x00, 0x19, 0x02, 0xD0, 0x02, 0xD0,
			0x02, 0x00, 0x02, 0x86, 0x00, 0x20, 0x02, 0x9E,
			0x00, 0x0A, 0x00, 0x0D, 0x04, 0x56, 0x03, 0x0D,
			0x18, 0x00, 0x10, 0xF0, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A, 0x40,
			0x2A, 0xBE, 0x3A, 0xFC, 0x3A, 0xFA, 0x3A, 0xF8,
			0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6,
			0x4B, 0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x9D, 01, {0x01}},
	/* CASET/PASET Setting 1440*3200 */
	{0x2A, 04, {0x00, 0x00, 0x05, 0x9F}},
	{0x2B, 04, {0x00, 0x00, 0x0C, 0x7F}},
	/* scaler setting WQHD */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0C}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* TE fixed off*/
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_fhdp_setting[] = {
	/* FHDP DSC setting */
	{0x9E, 128, {0x12, 0x00, 0x00, 0xAB, 0x30, 0x80, 0x09, 0x60,
			0x04, 0x38, 0x00, 0x20, 0x02, 0x1C, 0x02, 0x1C,
			0x02, 0x00, 0x02, 0x3B, 0x00, 0x20, 0x02, 0xD9,
			0x00, 0x07, 0x00, 0x0E, 0x03, 0x9D, 0x03, 0x2E,
			0x18, 0x00, 0x10, 0xF0, 0x07, 0x10, 0x20, 0x00,
			0x06, 0x0F, 0x0F, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x02, 0x02, 0x22, 0x00, 0x2A, 0x40,
			0x2A, 0xBE, 0x3A, 0xFC, 0x3A, 0xFA, 0x3A, 0xF8,
			0x3B, 0x38, 0x3B, 0x78, 0x3B, 0xB6, 0x4B, 0xB6,
			0x4B, 0xF4, 0x4B, 0xF4, 0x6C, 0x34, 0x84, 0x74,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x9D, 01, {0x01}},
	/* CASET/PASET Setting 1080*2400 */
	{0x2A, 04, {0x00, 0x00, 0x04, 0x37}},
	{0x2B, 04, {0x00, 0x00, 0x09, 0x5F}},
	/* scaler setting FHDP */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x0D}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* TE fixed off*/
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB9, 01, {0x00}},
	{0xB0, 03, {0x00, 0x06, 0xB9}},
	{0xB9, 06, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_err("[lh]debug for l2m_38_0a_0a_dsc_cmd lcm %s  ctx->prepared %d \n", __func__,ctx->prepared);

	if (ctx->prepared)
		return 0;

	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	udelay(1000);

	ctx->dvdd_gpio = devm_gpiod_get_index(ctx->dev,
	    "dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
	    dev_err(ctx->dev, "%s: cannot get dvdd gpio %ld\n",
	        __func__, PTR_ERR(ctx->dvdd_gpio));
	    return PTR_ERR(ctx->dvdd_gpio);
	}
	gpiod_set_value(ctx->dvdd_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->dvdd_gpio);

	/* move vddi power off to here for after mipi power off*/
	udelay(2000);
	lcm_panel_vci_disable(ctx->dev);
	udelay(2000);
	lcm_panel_vddi_disable(ctx->dev);
	udelay(2000);

	return 0;
}

static int mode_switch_start(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage, int fps)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		switch (fps) {
			case 60:
				if (ctx->dynamic_fps == 90) {
					push_table(ctx, mode_60hz_setting_exit_90hz,
						sizeof(mode_60hz_setting_exit_90hz) / sizeof(struct LCM_setting_table));
				} else {
					push_table(ctx, mode_60hz_setting,
						sizeof(mode_60hz_setting) / sizeof(struct LCM_setting_table));
				}
				break;
			case 120:
				if (ctx->dynamic_fps == 90) {
					push_table(ctx, mode_120hz_setting_exit_90hz,
						sizeof(mode_120hz_setting_exit_90hz) / sizeof(struct LCM_setting_table));
				} else {
					push_table(ctx, mode_120hz_setting,
						sizeof(mode_120hz_setting) / sizeof(struct LCM_setting_table));
				}
				break;
			case 90:
				if (ctx->dynamic_fps == 60) {
					push_table(ctx, mode_90hz_front_setting,
						sizeof(mode_90hz_front_setting) / sizeof(struct LCM_setting_table));
				} else {
					push_table(ctx, mode_90hz_setting,
						sizeof(mode_90hz_setting) / sizeof(struct LCM_setting_table));
				}
				break;
			case 40:
				if (ctx->dynamic_fps == 90) {
					push_table(ctx, mode_40hz_setting_exit_90hz,
						sizeof(mode_40hz_setting_exit_90hz) / sizeof(struct LCM_setting_table));
				} else {
					push_table(ctx, mode_40hz_setting,
						sizeof(mode_40hz_setting) / sizeof(struct LCM_setting_table));
				}
				break;
			case 30:
				if (ctx->dynamic_fps == 90) {
					push_table(ctx, mode_30hz_setting_exit_90hz,
						sizeof(mode_30hz_setting_exit_90hz) / sizeof(struct LCM_setting_table));
				} else {
					push_table(ctx, mode_30hz_setting,
						sizeof(mode_30hz_setting) / sizeof(struct LCM_setting_table));
				}
				break;
			case 24:
				if (ctx->dynamic_fps == 90) {
					push_table(ctx, mode_24hz_setting_exit_90hz,
						sizeof(mode_24hz_setting_exit_90hz) / sizeof(struct LCM_setting_table));
				} else {
					push_table(ctx, mode_24hz_setting,
						sizeof(mode_24hz_setting) / sizeof(struct LCM_setting_table));
				}
				break;
			case 10:
				if (ctx->dynamic_fps == 90) {
					push_table(ctx, mode_10hz_setting_exit_90hz,
						sizeof(mode_10hz_setting_exit_90hz) / sizeof(struct LCM_setting_table));
				} else {
					push_table(ctx, mode_10hz_setting,
						sizeof(mode_10hz_setting) / sizeof(struct LCM_setting_table));
				}
				break;
			case 1:
				if (ctx->dynamic_fps == 90) {
					push_table(ctx, mode_1hz_setting_exit_90hz,
						sizeof(mode_1hz_setting_exit_90hz) / sizeof(struct LCM_setting_table));
				} else {
					push_table(ctx, mode_1hz_setting,
						sizeof(mode_1hz_setting) / sizeof(struct LCM_setting_table));
				}
				break;
			default: return -1;
		}
	} else if (stage == AFTER_DSI_POWERON) {
		switch (fps) {
			case 90:
				if (ctx->dynamic_fps == 60)
					push_table(ctx, mode_90hz_back_setting,
						sizeof(mode_90hz_back_setting) / sizeof(struct LCM_setting_table));
				break;
			case 120:
			case 60:
			case 40:
			case 30:
			case 24:
			case 10:
			case 1:
				break;
			default: return -1;
		}
		ctx->dynamic_fps = fps;
	}
	return 0;
}

static void mode_switch_to_wqhd(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		push_table(ctx, mode_wqhd_setting,
			sizeof(mode_wqhd_setting) / sizeof(struct LCM_setting_table));
	}
	ctx->wqhd_en = true;
}

static void mode_switch_to_fhdp(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		push_table(ctx, mode_fhdp_setting,
			sizeof(mode_fhdp_setting) / sizeof(struct LCM_setting_table));
	}
	ctx->wqhd_en = false;
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	int ret1 = 0;
	int ret2 = 0;
	bool isFpsChange = false;
	bool isResChange = false;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);

	if (cur_mode == dst_mode)
		return ret;

	isFpsChange = drm_mode_vrefresh(m_dst) == drm_mode_vrefresh(m_cur)? false: true;
	isResChange = m_dst->vdisplay == m_cur->vdisplay? false: true;

	pr_info("%s isFpsChange = %d, isResChange = %d\n", __func__, isFpsChange, isResChange);
	pr_info("%s dst_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_dst), m_dst->vdisplay, m_dst->hdisplay);
	pr_info("%s cur_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_cur), m_cur->vdisplay, m_cur->hdisplay);

	if (isFpsChange) {
		ret1 = mode_switch_start(panel, stage, drm_mode_vrefresh(m_dst));
	}

	if (isResChange) {
		if (m_dst->hdisplay == HACT_WQHD && m_dst->vdisplay == VACT_WQHD)
			mode_switch_to_wqhd(panel, stage);
		else if (m_dst->hdisplay == HACT_FHDP && m_dst->vdisplay == VACT_FHDP)
			mode_switch_to_fhdp(panel, stage);
		else
			ret2 = 1;
	}
	ret = ret1 || ret2;

	return ret;
}

#ifdef CONFIG_MI_DISP
static void lcm_panel_init(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s: +, dynamic_fps = %d\n", __func__, ctx->dynamic_fps);
	if (ctx->prepared) {
		pr_info("%s: panel has been prepared, nothing to do!\n", __func__);
		goto err;
	}

	if (ctx->dynamic_fps == 120) {
		if (init_setting_wqhd[FPS_INIT_INDEX].cmd == 0x60) {
			init_setting_wqhd[FPS_INIT_INDEX].para_list[0] = 0x00;
		}
		if (init_setting_fhdp[FPS_INIT_INDEX].cmd == 0x60) {
			init_setting_fhdp[FPS_INIT_INDEX].para_list[0] = 0x00;
		}
	} else if (ctx->dynamic_fps == 60) {
		if (init_setting_wqhd[FPS_INIT_INDEX].cmd == 0x60) {
			init_setting_wqhd[FPS_INIT_INDEX].para_list[0] = 0x01;
		}
		if (init_setting_fhdp[FPS_INIT_INDEX].cmd == 0x60) {
			init_setting_fhdp[FPS_INIT_INDEX].para_list[0] = 0x01;
		}
	} else if (ctx->dynamic_fps == 40) {
		if (init_setting_wqhd[FPS_INIT_INDEX].cmd == 0x60) {
			init_setting_wqhd[FPS_INIT_INDEX].para_list[0] = 0x02;
		}
		if (init_setting_fhdp[FPS_INIT_INDEX].cmd == 0x60) {
			init_setting_fhdp[FPS_INIT_INDEX].para_list[0] = 0x02;
		}
	} else if (ctx->dynamic_fps == 30) {
		if (init_setting_wqhd[FPS_INIT_INDEX].cmd == 0x60) {
			init_setting_wqhd[FPS_INIT_INDEX].para_list[0] = 0x04;
		}
		if (init_setting_fhdp[FPS_INIT_INDEX].cmd == 0x60) {
			init_setting_fhdp[FPS_INIT_INDEX].para_list[0] = 0x04;
		}
	} else if (ctx->dynamic_fps == 24) {
		if (init_setting_wqhd[FPS_INIT_INDEX].cmd == 0x60) {
			init_setting_wqhd[FPS_INIT_INDEX].para_list[0] = 0x05;
		}
		if (init_setting_fhdp[FPS_INIT_INDEX].cmd == 0x60) {
			init_setting_fhdp[FPS_INIT_INDEX].para_list[0] = 0x05;
		}
	} else if (ctx->dynamic_fps == 10) {
		if (init_setting_fixte_wqhd[FIXTE_FPS_INIT_INDEX].cmd == 0xBD) {
			init_setting_fixte_wqhd[FIXTE_FPS_INIT_INDEX].para_list[0] = 0x0B;
		}
		if (init_setting_fixte_fhdp[FIXTE_FPS_INIT_INDEX].cmd == 0xBD) {
			init_setting_fixte_fhdp[FIXTE_FPS_INIT_INDEX].para_list[0] = 0x0B;
		}
	} else if (ctx->dynamic_fps == 1) {
		if (init_setting_fixte_wqhd[FIXTE_FPS_INIT_INDEX].cmd == 0xBD) {
			init_setting_fixte_wqhd[FIXTE_FPS_INIT_INDEX].para_list[0] = 0x77;
		}
		if (init_setting_fixte_fhdp[FIXTE_FPS_INIT_INDEX].cmd == 0xBD) {
			init_setting_fixte_fhdp[FIXTE_FPS_INIT_INDEX].para_list[0] = 0x77;
		}
	}

	if (ctx->wqhd_en) {
		if (ctx->dynamic_fps != 90 && ctx->dynamic_fps != 10 && ctx->dynamic_fps != 1) {
			push_table(ctx, init_setting_wqhd,
					sizeof(init_setting_wqhd) / sizeof(struct LCM_setting_table));
		} else if (ctx->dynamic_fps == 90) {
			push_table(ctx, init_setting_90hz_wqhd,
					sizeof(init_setting_90hz_wqhd) / sizeof(struct LCM_setting_table));
		} else if (ctx->dynamic_fps == 10 || ctx->dynamic_fps == 1) {
			push_table(ctx, init_setting_fixte_wqhd,
					sizeof(init_setting_fixte_wqhd) / sizeof(struct LCM_setting_table));
		}
	} else {
		if (ctx->dynamic_fps != 90) {
			push_table(ctx, init_setting_fhdp,
					sizeof(init_setting_fhdp) / sizeof(struct LCM_setting_table));
		} else if (ctx->dynamic_fps == 90) {
			push_table(ctx, init_setting_90hz_fhdp,
					sizeof(init_setting_90hz_fhdp) / sizeof(struct LCM_setting_table));
		} else if (ctx->dynamic_fps == 10 || ctx->dynamic_fps == 1) {
			push_table(ctx, init_setting_fixte_fhdp,
					sizeof(init_setting_fixte_fhdp) / sizeof(struct LCM_setting_table));
		}
	}

	if (ctx->gir_status) {
		push_table(ctx, init_setting_gir_on,
				sizeof(init_setting_gir_on) / sizeof(struct LCM_setting_table));
	} else {
		push_table(ctx, init_setting_gir_off,
				sizeof(init_setting_gir_off) / sizeof(struct LCM_setting_table));
	}

	ctx->prepared = true;
err:
	pr_info("%s: -\n", __func__);
}

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

static void panel_set_dc_backlight(struct drm_panel *panel, int brightness)
{
	int ret;
	char brightness_cmd0[] = {0x51, 0x00, 0x00};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 1,
	};

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}
	pr_info("%s brightness = %d", __func__, brightness);

	if (brightness) {
		bl_tb0[1] = (brightness >> 8) & 0xFF;
		bl_tb0[2] = brightness & 0xFF;
	}

	brightness_cmd0[1] = (brightness >> 8) & 0xFF;
	brightness_cmd0[2] = brightness & 0xFF;

	cmd_msg.type[0] = ARRAY_SIZE(brightness_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = brightness_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(brightness_cmd0);

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, false, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
	pr_info("%s exit\n", __func__);
}

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
	struct lcm *ctx;

	int ret;

	char AOD_setting_hbm_cmd0[] = {0xF0, 0x5A, 0x5A};
	char AOD_setting_hbm_cmd1[] = {0x53, 0x24};
	char AOD_setting_hbm_cmd2[] = {0x51, 0x07, 0xFF};
	char AOD_setting_hbm_cmd3[] = {0xF0, 0xA5, 0xA5};

	char AOD_setting_lbm_cmd0[] = {0xF0, 0x5A, 0x5A};
	char AOD_setting_lbm_cmd1[] = {0x53, 0x24};
	char AOD_setting_lbm_cmd2[] = {0x51, 0x00, 0xF6};
	char AOD_setting_lbm_cmd3[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 4,
	};

	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}

	ctx = panel_to_lcm(panel);
	if (ctx->doze_brightness_state == doze_brightness) {
		pr_info("%s skip same doze_brightness set:%d\n", __func__, doze_brightness);
		return 0;
	}

	pr_info("%s+ doze_brightness:%d\n", __func__, doze_brightness);

	if (DOZE_TO_NORMAL == doze_brightness) {
		atomic_set(&doze_enable, 0);
		goto exit;
	}

	if (atomic_read(&lhbm_enable)) {
		pr_info("%s skip doze_brightness set:%d, lhbm is on\n", __func__, doze_brightness);
		goto exit;
	}


	if (atomic_read(&doze_enable) == 1)
		atomic_set(&doze_enable, 2);

	if(atomic_read(&doze_enable) == 0) {
		pr_info("%s doze_brightness_state is DOZE_TO_NORMAL\n",__func__);
		goto exit;
	}

	if (doze_brightness == DOZE_BRIGHTNESS_HBM) {
		cmd_msg.type[0] = ARRAY_SIZE(AOD_setting_hbm_cmd0) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[0] = AOD_setting_hbm_cmd0;
		cmd_msg.tx_len[0] = ARRAY_SIZE(AOD_setting_hbm_cmd0);

		cmd_msg.type[1] = ARRAY_SIZE(AOD_setting_hbm_cmd1) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[1] = AOD_setting_hbm_cmd1;
		cmd_msg.tx_len[1] = ARRAY_SIZE(AOD_setting_hbm_cmd1);

		cmd_msg.type[2] = ARRAY_SIZE(AOD_setting_hbm_cmd2) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[2] = AOD_setting_hbm_cmd2;
		cmd_msg.tx_len[2] = ARRAY_SIZE(AOD_setting_hbm_cmd2);

		cmd_msg.type[3] = ARRAY_SIZE(AOD_setting_hbm_cmd3) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[3] = AOD_setting_hbm_cmd3;
		cmd_msg.tx_len[3] = ARRAY_SIZE(AOD_setting_hbm_cmd3);
	} else {
		cmd_msg.type[0] = ARRAY_SIZE(AOD_setting_lbm_cmd0) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[0] = AOD_setting_lbm_cmd0;
		cmd_msg.tx_len[0] = ARRAY_SIZE(AOD_setting_lbm_cmd0);

		cmd_msg.type[1] = ARRAY_SIZE(AOD_setting_lbm_cmd1) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[1] = AOD_setting_lbm_cmd1;
		cmd_msg.tx_len[1] = ARRAY_SIZE(AOD_setting_lbm_cmd1);

		cmd_msg.type[2] = ARRAY_SIZE(AOD_setting_lbm_cmd2) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[2] = AOD_setting_lbm_cmd2;
		cmd_msg.tx_len[2] = ARRAY_SIZE(AOD_setting_lbm_cmd2);

		cmd_msg.type[3] = ARRAY_SIZE(AOD_setting_lbm_cmd3) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[3] = AOD_setting_lbm_cmd3;
		cmd_msg.tx_len[3] = ARRAY_SIZE(AOD_setting_lbm_cmd3);
	}

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
exit :
	ctx->doze_brightness_state = doze_brightness;
	pr_info("%s end -\n", __func__);
	return 0;
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
	return count;

}

static int panel_hbm_fod_control(struct drm_panel *panel, bool en)
{
	struct lcm *ctx;
	int ret = -1;
	char hbm_fod_on_cmd0[] = {0x53, 0xE0};
	char hbm_fod_on_cmd1[] = {0x51, 0x07, 0xFF};
	char hbm_fod_off_cmd0[] = {0x53, 0x20};
	char hbm_fod_off_cmd1[] = {0x51, 0x07, 0xFF};
	struct mtk_ddic_dsi_msg cmd_msg = {
	    .channel = 1,
	    .flags = 2,
	    .tx_cmd_num = 2,
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
		cmd_msg.type[0] = ARRAY_SIZE(hbm_fod_on_cmd0) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[0] = hbm_fod_on_cmd0;
		cmd_msg.tx_len[0] = ARRAY_SIZE(hbm_fod_on_cmd0);

		cmd_msg.type[1] = ARRAY_SIZE(hbm_fod_on_cmd1) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[1] = hbm_fod_on_cmd1;
		cmd_msg.tx_len[1] = ARRAY_SIZE(hbm_fod_on_cmd1);
	} else {
		hbm_fod_off_cmd1[1] = bl_tb0[1];
		hbm_fod_off_cmd1[2] = bl_tb0[2];
		cmd_msg.type[0] = ARRAY_SIZE(hbm_fod_off_cmd0) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[0] = hbm_fod_off_cmd0;
		cmd_msg.tx_len[0] = ARRAY_SIZE(hbm_fod_off_cmd0);

		cmd_msg.type[1] = ARRAY_SIZE(hbm_fod_off_cmd1) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[1] = hbm_fod_off_cmd1;
		cmd_msg.tx_len[1] = ARRAY_SIZE(hbm_fod_off_cmd1);
	}
	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int local_hbm_hlpm_white(struct mtk_dsi *dsi, enum local_hbm_state lhbm_state)
{
	int ret = -1;
	struct lcm *ctx;

	char local_hbm_hlpm_white_cmd0[] = {0xF0, 0x5A, 0x5A};
	char local_hbm_hlpm_white_cmd1[] = {0x51, 0x00, 0xE5};
	char local_hbm_hlpm_white_cmd2[] = {0x53, 0x20};
	char local_hbm_hlpm_white_cmd3[] = {0xF0, 0xA5, 0xA5};
	char local_hbm_hlpm_white_cmd4[] = {0xF0, 0x5A, 0x5A};
	char local_hbm_hlpm_white_cmd5[] = {0xB0, 0x01, 0xDC, 0x1F};
	char local_hbm_hlpm_white_cmd6[] = {0x1F, 0x02, 0x00, 0x00, 0x00, 0x00, 0x24, 0x09, 0x1F, 0x35, 0xFA, 0x3F, 0x2D, 0x09, 0xAF, 0x86};
	char local_hbm_hlpm_white_cmd7[] = {0xB0, 0x02, 0xAC, 0x66};
	char local_hbm_hlpm_white_cmd8[] = {0x66, 0x0F, 0xFF};
	char local_hbm_hlpm_white_cmd9[] = {0xB0, 0x01, 0x6D, 0x66};
	char local_hbm_hlpm_white_cmd10[] = {0x66, 0x00, 0x40, 0x14, 0x02, 0x90, 0x52, 0x0A, 0x41, 0x48, 0x1C, 0x27, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	char local_hbm_hlpm_white_cmd11[] = {0xB0, 0x01, 0x59, 0x66};

	char local_hbm_hlpm_white_1000nit_cmd12[] = {0x66, 0x08, 0x7A, 0x9F, 0xF5, 0xAF, 0x08, 0x7A, 0x9F, 0xF5, 0xAF};
	char local_hbm_hlpm_white_110nit_cmd12[] = {0x66, 0x05, 0x56, 0xE0, 0x83, 0xE8, 0x05, 0x56, 0xE0, 0x83, 0xE8};

	char local_hbm_hlpm_white_cmd13[] = {0xB0, 0x01, 0xB5, 0x66};
	char local_hbm_hlpm_white_cmd14[] = {0x66, 0x14, 0x01, 0xDE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00, 0x80, 0x08, 0x00, 0x80,
		                                           0x08, 0x00, 0x14, 0x01, 0xDE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00, 0x80, 0x08,
		                                           0x00, 0x80, 0x08, 0x00, 0x14, 0x01, 0xDE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00,
		                                           0x80, 0x08, 0x00, 0x80, 0x08, 0x00};
	char local_hbm_hlpm_white_cmd15[] = {0xB0, 0x01, 0xEB, 0x66};
	char local_hbm_hlpm_white_cmd16[] = {0x66, 0x14, 0x61, 0xEE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00, 0x80, 0x08, 0x00, 0x80,
		                                           0x08, 0x00, 0x14, 0x61, 0xEE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00, 0x80, 0x08,
		                                           0x00, 0x80, 0x08, 0x00, 0x14, 0x61, 0xEE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00,
		                                           0x80, 0x08, 0x00, 0x80, 0x08, 0x00};
	char local_hbm_hlpm_white_cmd17[] = {0x53, 0x30};
	char local_hbm_hlpm_white_cmd18[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 19,
	};

	ctx = panel_to_lcm(dsi->panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}

	pr_info("%s: +\n", __func__);

	cmd_msg.type[0] = ARRAY_SIZE(local_hbm_hlpm_white_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = local_hbm_hlpm_white_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(local_hbm_hlpm_white_cmd0);

	pr_info("%s: doze_enable = %d\n", __func__, atomic_read(&doze_enable));
	if (atomic_read(&doze_enable)) {
		pr_info("%s: doze_brightness_state: %d\n", __func__, ctx->doze_brightness_state);
		switch (ctx->doze_brightness_state) {
		case DOZE_BRIGHTNESS_HBM:
			local_hbm_hlpm_white_cmd1[1] = (DOZE_HBM_DBV_LEVEL >> 8) & 0xFF;
			local_hbm_hlpm_white_cmd1[2] = DOZE_HBM_DBV_LEVEL & 0xFF;
			break;
		case DOZE_BRIGHTNESS_LBM:
			local_hbm_hlpm_white_cmd1[1] = (DOZE_LBM_DBV_LEVEL >> 8) & 0xFF;
			local_hbm_hlpm_white_cmd1[2] = DOZE_LBM_DBV_LEVEL & 0xFF;
			break;
		}
	}

	cmd_msg.type[1] = ARRAY_SIZE(local_hbm_hlpm_white_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = local_hbm_hlpm_white_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(local_hbm_hlpm_white_cmd1);

	cmd_msg.type[2] = ARRAY_SIZE(local_hbm_hlpm_white_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[2] = local_hbm_hlpm_white_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(local_hbm_hlpm_white_cmd2);

	cmd_msg.type[3] = ARRAY_SIZE(local_hbm_hlpm_white_cmd3) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[3] = local_hbm_hlpm_white_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(local_hbm_hlpm_white_cmd3);

	cmd_msg.type[4] = ARRAY_SIZE(local_hbm_hlpm_white_cmd4) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[4] = local_hbm_hlpm_white_cmd4;
	cmd_msg.tx_len[4] = ARRAY_SIZE(local_hbm_hlpm_white_cmd4);

	cmd_msg.type[5] = ARRAY_SIZE(local_hbm_hlpm_white_cmd5) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[5] = local_hbm_hlpm_white_cmd5;
	cmd_msg.tx_len[5] = ARRAY_SIZE(local_hbm_hlpm_white_cmd5);

	cmd_msg.type[6] = ARRAY_SIZE(local_hbm_hlpm_white_cmd6) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[6] = local_hbm_hlpm_white_cmd6;
	cmd_msg.tx_len[6] = ARRAY_SIZE(local_hbm_hlpm_white_cmd6);

	cmd_msg.type[7] = ARRAY_SIZE(local_hbm_hlpm_white_cmd7) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[7] = local_hbm_hlpm_white_cmd7;
	cmd_msg.tx_len[7] = ARRAY_SIZE(local_hbm_hlpm_white_cmd7);

	cmd_msg.type[8] = ARRAY_SIZE(local_hbm_hlpm_white_cmd8) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[8] = local_hbm_hlpm_white_cmd8;
	cmd_msg.tx_len[8] = ARRAY_SIZE(local_hbm_hlpm_white_cmd8);

	cmd_msg.type[9] = ARRAY_SIZE(local_hbm_hlpm_white_cmd9) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[9] = local_hbm_hlpm_white_cmd9;
	cmd_msg.tx_len[9] = ARRAY_SIZE(local_hbm_hlpm_white_cmd9);

	cmd_msg.type[10] = ARRAY_SIZE(local_hbm_hlpm_white_cmd10) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[10] = local_hbm_hlpm_white_cmd10;
	cmd_msg.tx_len[10] = ARRAY_SIZE(local_hbm_hlpm_white_cmd10);

	cmd_msg.type[11] = ARRAY_SIZE(local_hbm_hlpm_white_cmd11) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[11] = local_hbm_hlpm_white_cmd11;
	cmd_msg.tx_len[11] = ARRAY_SIZE(local_hbm_hlpm_white_cmd11);

	if (lhbm_state == LOCAL_HBM_HLPM_WHITE_1000NIT) {
		cmd_msg.type[12] = ARRAY_SIZE(local_hbm_hlpm_white_1000nit_cmd12) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[12] = local_hbm_hlpm_white_1000nit_cmd12;
		cmd_msg.tx_len[12] = ARRAY_SIZE(local_hbm_hlpm_white_1000nit_cmd12);
	} else if (lhbm_state == LOCAL_HBM_HLPM_WHITE_110NIT) {
		cmd_msg.type[12] = ARRAY_SIZE(local_hbm_hlpm_white_110nit_cmd12) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[12] = local_hbm_hlpm_white_110nit_cmd12;
		cmd_msg.tx_len[12] = ARRAY_SIZE(local_hbm_hlpm_white_110nit_cmd12);
	}

	cmd_msg.type[13] = ARRAY_SIZE(local_hbm_hlpm_white_cmd13) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[13] = local_hbm_hlpm_white_cmd13;
	cmd_msg.tx_len[13] = ARRAY_SIZE(local_hbm_hlpm_white_cmd13);

	cmd_msg.type[14] = ARRAY_SIZE(local_hbm_hlpm_white_cmd14) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[14] = local_hbm_hlpm_white_cmd14;
	cmd_msg.tx_len[14] = ARRAY_SIZE(local_hbm_hlpm_white_cmd14);

	cmd_msg.type[15] = ARRAY_SIZE(local_hbm_hlpm_white_cmd15) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[15] = local_hbm_hlpm_white_cmd15;
	cmd_msg.tx_len[15] = ARRAY_SIZE(local_hbm_hlpm_white_cmd15);

	cmd_msg.type[16] = ARRAY_SIZE(local_hbm_hlpm_white_cmd16) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[16] = local_hbm_hlpm_white_cmd16;
	cmd_msg.tx_len[16] = ARRAY_SIZE(local_hbm_hlpm_white_cmd16);

	cmd_msg.type[17] = ARRAY_SIZE(local_hbm_hlpm_white_cmd17) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[17] = local_hbm_hlpm_white_cmd17;
	cmd_msg.tx_len[17] = ARRAY_SIZE(local_hbm_hlpm_white_cmd17);

	cmd_msg.type[18] = ARRAY_SIZE(local_hbm_hlpm_white_cmd18) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[18] = local_hbm_hlpm_white_cmd18;
	cmd_msg.tx_len[18] = ARRAY_SIZE(local_hbm_hlpm_white_cmd18);

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int local_hbm_normal_white(struct mtk_dsi *dsi, enum local_hbm_state lhbm_state)
{
	int ret = -1;
	struct lcm *ctx;

	char local_hbm_normal_cmd0[] = {0xF0, 0x5A, 0x5A};
	char local_hbm_normal_cmd1[] = {0xB0, 0x01, 0xDC, 0x1F};
	char local_hbm_normal_cmd2[] = {0x1F, 0x02, 0x00, 0x00, 0x00, 0x00, 0x24, 0x09, 0x1F, 0x35, 0xFA, 0x3F, 0x2D, 0x09, 0xAF, 0x86};
	char local_hbm_normal_cmd3[] = {0xB0, 0x02, 0xAC, 0x66};
	char local_hbm_normal_cmd4[] = {0x66, 0x0F, 0xFF};
	char local_hbm_normal_cmd5[] = {0xB0, 0x01, 0x6D, 0x66};
	char local_hbm_normal_cmd6[] = {0x66, 0x00, 0x40, 0x14, 0x02, 0x90, 0x52, 0x0A, 0x41, 0x48, 0x1C, 0x27, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	char local_hbm_normal_cmd7[] = {0xB0, 0x01, 0x59, 0x66};

	char local_hbm_normal_white_1000nit_cmd8[] = {0x66, 0x08, 0x7A, 0x9F, 0xF5, 0xAF, 0x08, 0x7A, 0x9F, 0xF5, 0xAF};
	char local_hbm_normal_white_750nit_cmd8[] = {0x66, 0x08, 0x79, 0x23, 0x8B, 0xE8, 0x08, 0x79, 0x23, 0x8B, 0xE8};
	char local_hbm_normal_white_500nit_cmd8[] = {0x66, 0x07, 0x79, 0x93, 0x0A, 0x18, 0x07, 0x79, 0x93, 0x0A, 0x18};
	char local_hbm_normal_white_110nit_cmd8[] = {0x66, 0x05, 0x56, 0xE0, 0x83, 0xE8, 0x05, 0x56, 0xE0, 0x83, 0xE8};
	char local_hbm_normal_green_500nit_cmd8[] = {0x66, 0x00, 0x70, 0x00, 0x5C, 0x00, 0x00, 0x70, 0x00, 0x5C, 0x00};

	char local_hbm_normal_cmd9[] = {0xB0, 0x01, 0xB5, 0x66};
	char local_hbm_normal_cmd10[] = {0x66, 0x14, 0x01, 0xDE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00, 0x80, 0x08, 0x00, 0x80,
		                                            0x08, 0x00, 0x14, 0x01, 0xDE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00, 0x80, 0x08,
		                                            0x00, 0x80, 0x08, 0x00, 0x14, 0x01, 0xDE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00,
		                                            0x80, 0x08, 0x00, 0x80, 0x08, 0x00};
	char local_hbm_normal_cmd11[] = {0xB0, 0x01, 0xEB, 0x66};
	char local_hbm_normal_cmd12[] = {0x66, 0x14, 0x61, 0xEE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00, 0x80, 0x08, 0x00, 0x80,
		                                            0x08, 0x00, 0x14, 0x61, 0xEE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00, 0x80, 0x08,
		                                            0x00, 0x80, 0x08, 0x00, 0x14, 0x61, 0xEE, 0x2A, 0x43, 0xA2, 0x4F, 0x66, 0xCC, 0x80, 0x08, 0x00,
		                                            0x80, 0x08, 0x00, 0x80, 0x08, 0x00};
	char local_hbm_normal_cmd13[] = {0x53, 0x30};
	char local_hbm_normal_white_1000nit_cmd14[] = {0x51, 0x14, 0x02};
	char local_hbm_normal_cmd_end[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 15,
	};

	pr_info("%s: +\n", __func__);

	ctx = panel_to_lcm(dsi->panel);

	cmd_msg.type[0] = ARRAY_SIZE(local_hbm_normal_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = local_hbm_normal_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(local_hbm_normal_cmd0);

	cmd_msg.type[1] = ARRAY_SIZE(local_hbm_normal_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = local_hbm_normal_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(local_hbm_normal_cmd1);

	cmd_msg.type[2] = ARRAY_SIZE(local_hbm_normal_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[2] = local_hbm_normal_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(local_hbm_normal_cmd2);

	cmd_msg.type[3] = ARRAY_SIZE(local_hbm_normal_cmd3) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[3] = local_hbm_normal_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(local_hbm_normal_cmd3);

	cmd_msg.type[4] = ARRAY_SIZE(local_hbm_normal_cmd4) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[4] = local_hbm_normal_cmd4;
	cmd_msg.tx_len[4] = ARRAY_SIZE(local_hbm_normal_cmd4);

	cmd_msg.type[5] = ARRAY_SIZE(local_hbm_normal_cmd5) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[5] = local_hbm_normal_cmd5;
	cmd_msg.tx_len[5] = ARRAY_SIZE(local_hbm_normal_cmd5);

	cmd_msg.type[6] = ARRAY_SIZE(local_hbm_normal_cmd6) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[6] = local_hbm_normal_cmd6;
	cmd_msg.tx_len[6] = ARRAY_SIZE(local_hbm_normal_cmd6);

	cmd_msg.type[7] = ARRAY_SIZE(local_hbm_normal_cmd7) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[7] = local_hbm_normal_cmd7;
	cmd_msg.tx_len[7] = ARRAY_SIZE(local_hbm_normal_cmd7);

	msleep(20);
	if (lhbm_state == LOCAL_HBM_NORMAL_WHITE_1000NIT) {
		cmd_msg.type[8] = ARRAY_SIZE(local_hbm_normal_white_1000nit_cmd8) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[8] = local_hbm_normal_white_1000nit_cmd8;
		cmd_msg.tx_len[8] = ARRAY_SIZE(local_hbm_normal_white_1000nit_cmd8);
	} else if (lhbm_state == LOCAL_HBM_NORMAL_WHITE_750NIT) {
		cmd_msg.type[8] = ARRAY_SIZE(local_hbm_normal_white_750nit_cmd8) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[8] = local_hbm_normal_white_750nit_cmd8;
		cmd_msg.tx_len[8] = ARRAY_SIZE(local_hbm_normal_white_750nit_cmd8);
	} else if (lhbm_state == LOCAL_HBM_NORMAL_WHITE_500NIT) {
		cmd_msg.type[8] = ARRAY_SIZE(local_hbm_normal_white_500nit_cmd8) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[8] = local_hbm_normal_white_500nit_cmd8;
		cmd_msg.tx_len[8] = ARRAY_SIZE(local_hbm_normal_white_500nit_cmd8);
	} else if (lhbm_state == LOCAL_HBM_NORMAL_WHITE_110NIT) {
		cmd_msg.type[8] = ARRAY_SIZE(local_hbm_normal_white_110nit_cmd8) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[8] = local_hbm_normal_white_110nit_cmd8;
		cmd_msg.tx_len[8] = ARRAY_SIZE(local_hbm_normal_white_110nit_cmd8);
	} else if (lhbm_state == LOCAL_HBM_NORMAL_GREEN_500NIT) {
		cmd_msg.type[8] = ARRAY_SIZE(local_hbm_normal_green_500nit_cmd8) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[8] = local_hbm_normal_green_500nit_cmd8;
		cmd_msg.tx_len[8] = ARRAY_SIZE(local_hbm_normal_green_500nit_cmd8);
	}
	cmd_msg.type[9] = ARRAY_SIZE(local_hbm_normal_cmd9) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[9] = local_hbm_normal_cmd9;
	cmd_msg.tx_len[9] = ARRAY_SIZE(local_hbm_normal_cmd9);

	cmd_msg.type[10] = ARRAY_SIZE(local_hbm_normal_cmd10) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[10] = local_hbm_normal_cmd10;
	cmd_msg.tx_len[10] = ARRAY_SIZE(local_hbm_normal_cmd10);

	cmd_msg.type[11] = ARRAY_SIZE(local_hbm_normal_cmd11) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[11] = local_hbm_normal_cmd11;
	cmd_msg.tx_len[11] = ARRAY_SIZE(local_hbm_normal_cmd11);

	cmd_msg.type[12] = ARRAY_SIZE(local_hbm_normal_cmd12) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[12] = local_hbm_normal_cmd12;
	cmd_msg.tx_len[12] = ARRAY_SIZE(local_hbm_normal_cmd12);

	cmd_msg.type[13] = ARRAY_SIZE(local_hbm_normal_cmd13) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[13] = local_hbm_normal_cmd13;
	cmd_msg.tx_len[13] = ARRAY_SIZE(local_hbm_normal_cmd13);

	if (lhbm_state == LOCAL_HBM_NORMAL_WHITE_1000NIT) {
		if (ctx->hbm_en) {
			local_hbm_normal_white_1000nit_cmd14[1] = 0x03;
			local_hbm_normal_white_1000nit_cmd14[2] = 0xFF;
		} else {
			if (atomic_read(&doze_enable)) {
				switch (ctx->doze_brightness_state) {
				case DOZE_BRIGHTNESS_HBM:
					local_hbm_normal_white_1000nit_cmd14[1] = (DOZE_HBM_DBV_LEVEL >> 8) & 0xFF;
					local_hbm_normal_white_1000nit_cmd14[2] = DOZE_HBM_DBV_LEVEL & 0xFF;
					break;
				case DOZE_BRIGHTNESS_LBM:
					local_hbm_normal_white_1000nit_cmd14[1] = (DOZE_LBM_DBV_LEVEL >> 8) & 0xFF;
					local_hbm_normal_white_1000nit_cmd14[2] = DOZE_LBM_DBV_LEVEL & 0xFF;
					break;
				}
			} else {
				local_hbm_normal_white_1000nit_cmd14[1] = (dsi->mi_cfg.last_bl_level >> 8) & 0xFF;
				local_hbm_normal_white_1000nit_cmd14[2] = dsi->mi_cfg.last_bl_level & 0xFF;
			}
		}
		cmd_msg.type[14] = ARRAY_SIZE(local_hbm_normal_white_1000nit_cmd14) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[14] = local_hbm_normal_white_1000nit_cmd14;
		cmd_msg.tx_len[14] = ARRAY_SIZE(local_hbm_normal_white_1000nit_cmd14);
		cmd_msg.type[15] = ARRAY_SIZE(local_hbm_normal_cmd_end) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[15] = local_hbm_normal_cmd_end;
		cmd_msg.tx_len[15] = ARRAY_SIZE(local_hbm_normal_cmd_end);
		cmd_msg.tx_cmd_num = 16;
	} else {
		cmd_msg.type[14] = ARRAY_SIZE(local_hbm_normal_cmd_end) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[14] = local_hbm_normal_cmd_end;
		cmd_msg.tx_len[14] = ARRAY_SIZE(local_hbm_normal_cmd_end);
	}
	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
		atomic_set(&lhbm_enable, 0);
	}
	pr_info("%s: -\n", __func__);
	return ret;
}

static int local_hbm_off_to_aod(enum local_hbm_state lhbm_state)
{
	int ret = -1;

	char local_hbm_off_to_aod_cmd0[] = {0xF0, 0x5A, 0x5A};
	char local_hbm_off_to_aod_cmd1[] = {0xBB, 0x31};
	char local_hbm_off_to_aod_cmd2[] = {0xF7, 0x0F};
	char local_hbm_off_to_aod_cmd3[] = {0xF0, 0xA5, 0xA5};
	char local_hbm_off_to_aod_cmd4[] = {0xF0, 0x5A, 0x5A};
	char local_hbm_off_to_aod_cmd5[] = {0x53, 0x24};
	char local_hbm_off_to_hlpm_cmd6[] = {0x51, 0x07, 0xFF};
	char local_hbm_off_to_llpm_cmd6[] = {0xF0, 0xA5, 0xA5};
	char local_hbm_off_to_aod_cmd7[] = {0xF7, 0x0F};
	char local_hbm_off_to_aod_cmd8[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 9,
	};

	pr_info("%s: +\n", __func__);

	cmd_msg.type[0] = ARRAY_SIZE(local_hbm_off_to_aod_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = local_hbm_off_to_aod_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(local_hbm_off_to_aod_cmd0);

	cmd_msg.type[1] = ARRAY_SIZE(local_hbm_off_to_aod_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = local_hbm_off_to_aod_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(local_hbm_off_to_aod_cmd1);

	cmd_msg.type[2] = ARRAY_SIZE(local_hbm_off_to_aod_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[2] = local_hbm_off_to_aod_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(local_hbm_off_to_aod_cmd2);

	cmd_msg.type[3] = ARRAY_SIZE(local_hbm_off_to_aod_cmd3) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[3] = local_hbm_off_to_aod_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(local_hbm_off_to_aod_cmd3);

	cmd_msg.type[4] = ARRAY_SIZE(local_hbm_off_to_aod_cmd4) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[4] = local_hbm_off_to_aod_cmd4;
	cmd_msg.tx_len[4] = ARRAY_SIZE(local_hbm_off_to_aod_cmd4);

	cmd_msg.type[5] = ARRAY_SIZE(local_hbm_off_to_aod_cmd5) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[5] = local_hbm_off_to_aod_cmd5;
	cmd_msg.tx_len[5] = ARRAY_SIZE(local_hbm_off_to_aod_cmd5);

	if (lhbm_state == LOCAL_HBM_OFF_TO_HLPM) {
		cmd_msg.type[6] = ARRAY_SIZE(local_hbm_off_to_hlpm_cmd6) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[6] = local_hbm_off_to_hlpm_cmd6;
		cmd_msg.tx_len[6] = ARRAY_SIZE(local_hbm_off_to_hlpm_cmd6);
	} else {
		cmd_msg.type[6] = ARRAY_SIZE(local_hbm_off_to_llpm_cmd6) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[6] = local_hbm_off_to_llpm_cmd6;
		cmd_msg.tx_len[6] = ARRAY_SIZE(local_hbm_off_to_llpm_cmd6);
	}

	cmd_msg.type[7] = ARRAY_SIZE(local_hbm_off_to_aod_cmd7) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[7] = local_hbm_off_to_aod_cmd7;
	cmd_msg.tx_len[7] = ARRAY_SIZE(local_hbm_off_to_aod_cmd7);

	cmd_msg.type[8] = ARRAY_SIZE(local_hbm_off_to_aod_cmd8) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[8] = local_hbm_off_to_aod_cmd8;
	cmd_msg.tx_len[8] = ARRAY_SIZE(local_hbm_off_to_aod_cmd8);

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
	pr_info("%s: -\n", __func__);
	return ret;
}

static int local_hbm_off_to_normal(struct mtk_dsi *dsi, enum local_hbm_state lhbm_state)
{
	int ret = -1;
	struct lcm *ctx;

	char local_hbm_off_cmd0[] = {0xF0, 0x5A, 0x5A};
	char local_hbm_off_to_normal_cmd1[] = {0x53, 0x20};
	char local_hbm_off_to_hbm_cmd1[] = {0x53, 0xE8};
	char local_hbm_off_cmd2[] = {0x51, 0x00, 0x00};
	char local_hbm_off_end[] = {0xF0, 0xA5, 0xA5};

	char AOD_setting_hbm_cmd0[] = {0xF0, 0x5A, 0x5A};
	char AOD_setting_hbm_cmd1[] = {0xBB, 0x31};
	char AOD_setting_hbm_cmd2[] = {0xF7, 0x0F};
	char AOD_setting_hbm_cmd3[] = {0x53, 0x24};
	char AOD_setting_hbm_cmd4[] = {0x51, 0x07, 0xFE};
	char AOD_setting_hbm_cmd5[] = {0xF0, 0xA5, 0xA5};

	char AOD_setting_lbm_cmd0[] = {0xF0, 0x5A, 0x5A};
	char AOD_setting_lbm_cmd1[] = {0xBB, 0x31};
	char AOD_setting_lbm_cmd2[] = {0xF7, 0x0F};
	char AOD_setting_lbm_cmd3[] = {0x53, 0x24};
	char AOD_setting_lbm_cmd4[] = {0x51, 0x00, 0xF6};
	char AOD_setting_lbm_cmd5[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 3,
	};

	pr_info("%s: +\n", __func__);

	ctx = panel_to_lcm(dsi->panel);

	cmd_msg.type[0] = ARRAY_SIZE(local_hbm_off_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = local_hbm_off_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(local_hbm_off_cmd0);
	msleep(16);
	if(!atomic_read(&doze_enable) || lhbm_state == LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE) {
		if (lhbm_state == LOCAL_HBM_OFF_TO_NORMAL && ctx->hbm_en) {
			pr_info("%s setcmd of lhbm_off_to_hbm", __func__);
			cmd_msg.type[1] = ARRAY_SIZE(local_hbm_off_to_hbm_cmd1) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[1] = local_hbm_off_to_hbm_cmd1;
			cmd_msg.tx_len[1] = ARRAY_SIZE(local_hbm_off_to_hbm_cmd1);
			local_hbm_off_cmd2[1] = bl_tb0[1];
			local_hbm_off_cmd2[2] = bl_tb0[2];
			cmd_msg.type[2] = ARRAY_SIZE(local_hbm_off_cmd2) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[2] = local_hbm_off_cmd2;
			cmd_msg.tx_len[2] = ARRAY_SIZE(local_hbm_off_cmd2);
			cmd_msg.type[3] = ARRAY_SIZE(local_hbm_off_end) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[3] = local_hbm_off_end;
			cmd_msg.tx_len[3] = ARRAY_SIZE(local_hbm_off_end);
			cmd_msg.tx_cmd_num = 4;
		} else {
			pr_info("%s setcmd of lhbm_off_to_normal", __func__);
			cmd_msg.type[1] = ARRAY_SIZE(local_hbm_off_to_normal_cmd1) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[1] = local_hbm_off_to_normal_cmd1;
			cmd_msg.tx_len[1] = ARRAY_SIZE(local_hbm_off_to_normal_cmd1);
			cmd_msg.type[2] = ARRAY_SIZE(local_hbm_off_end) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[2] = local_hbm_off_end;
			cmd_msg.tx_len[2] = ARRAY_SIZE(local_hbm_off_end);
		}
		ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
		if (ret != 0) {
			DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
			return ret;
		}
	}
	if (lhbm_state == LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT) {
		if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM) {
			pr_info("%s setcmd of lhbm_to_hbm_aod", __func__);
			cmd_msg.type[0] = ARRAY_SIZE(AOD_setting_hbm_cmd0) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[0] = AOD_setting_hbm_cmd0;
			cmd_msg.tx_len[0] = ARRAY_SIZE(AOD_setting_hbm_cmd0);

			cmd_msg.type[1] = ARRAY_SIZE(AOD_setting_hbm_cmd1) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[1] = AOD_setting_hbm_cmd1;
			cmd_msg.tx_len[1] = ARRAY_SIZE(AOD_setting_hbm_cmd1);

			cmd_msg.type[2] = ARRAY_SIZE(AOD_setting_hbm_cmd2) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[2] = AOD_setting_hbm_cmd2;
			cmd_msg.tx_len[2] = ARRAY_SIZE(AOD_setting_hbm_cmd2);

			cmd_msg.type[3] = ARRAY_SIZE(AOD_setting_hbm_cmd3) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[3] = AOD_setting_hbm_cmd3;
			cmd_msg.tx_len[3] = ARRAY_SIZE(AOD_setting_hbm_cmd3);

			cmd_msg.type[4] = ARRAY_SIZE(AOD_setting_hbm_cmd4) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[4] = AOD_setting_hbm_cmd4;
			cmd_msg.tx_len[4] = ARRAY_SIZE(AOD_setting_hbm_cmd4);

			cmd_msg.type[5] = ARRAY_SIZE(AOD_setting_hbm_cmd5) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[5] = AOD_setting_hbm_cmd5;
			cmd_msg.tx_len[5] = ARRAY_SIZE(AOD_setting_hbm_cmd5);
		} else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM) {
			pr_info("%s setcmd of lhbm_to_lbm_aod", __func__);
			cmd_msg.type[0] = ARRAY_SIZE(AOD_setting_lbm_cmd0) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[0] = AOD_setting_lbm_cmd0;
			cmd_msg.tx_len[0] = ARRAY_SIZE(AOD_setting_lbm_cmd0);

			cmd_msg.type[1] = ARRAY_SIZE(AOD_setting_lbm_cmd1) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[1] = AOD_setting_lbm_cmd1;
			cmd_msg.tx_len[1] = ARRAY_SIZE(AOD_setting_lbm_cmd1);

			cmd_msg.type[2] = ARRAY_SIZE(AOD_setting_lbm_cmd2) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[2] = AOD_setting_lbm_cmd2;
			cmd_msg.tx_len[2] = ARRAY_SIZE(AOD_setting_lbm_cmd2);

			cmd_msg.type[3] = ARRAY_SIZE(AOD_setting_lbm_cmd3) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[3] = AOD_setting_lbm_cmd3;
			cmd_msg.tx_len[3] = ARRAY_SIZE(AOD_setting_lbm_cmd3);

			cmd_msg.type[4] = ARRAY_SIZE(AOD_setting_lbm_cmd4) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[4] = AOD_setting_lbm_cmd4;
			cmd_msg.tx_len[4] = ARRAY_SIZE(AOD_setting_lbm_cmd4);

			cmd_msg.type[5] = ARRAY_SIZE(AOD_setting_lbm_cmd5) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[5] = AOD_setting_lbm_cmd5;
			cmd_msg.tx_len[5] = ARRAY_SIZE(AOD_setting_lbm_cmd5);
		} else {
			DDPPR_ERR("%s: not in doze state\n", __func__);
			return ret;
		}
		cmd_msg.tx_cmd_num = 6;
		ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
		if (ret != 0) {
			DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
		}
	}

	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_set_lhbm_fod(struct mtk_dsi *dsi, enum local_hbm_state lhbm_state)
{
	int ret = -1;
	struct lcm *ctx;

	if (!dsi || !dsi->panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}

	ctx = panel_to_lcm(dsi->panel);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}

	switch (lhbm_state) {
		case LOCAL_HBM_OFF_TO_NORMAL:
		case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
		case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE: {
			ret = local_hbm_off_to_normal(dsi, lhbm_state);
			atomic_set(&lhbm_enable, 0);
		} break;
		case LOCAL_HBM_NORMAL_WHITE_1000NIT:
		case LOCAL_HBM_NORMAL_WHITE_750NIT:
		case LOCAL_HBM_NORMAL_WHITE_500NIT:
		case LOCAL_HBM_NORMAL_WHITE_110NIT:
		case LOCAL_HBM_NORMAL_GREEN_500NIT: {
			atomic_set(&lhbm_enable, 1);
			ret = local_hbm_normal_white(dsi, lhbm_state);
		} break;
		case LOCAL_HBM_HLPM_WHITE_1000NIT:
		case LOCAL_HBM_HLPM_WHITE_110NIT: {
			ret = local_hbm_hlpm_white(dsi, lhbm_state);
			atomic_set(&lhbm_enable, 1);
		} break;
		case LOCAL_HBM_OFF_TO_HLPM:
		case LOCAL_HBM_OFF_TO_LLPM:
			ret = local_hbm_off_to_aod(lhbm_state);
			break;
		default:
			break;
	}
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static void panel_set_dc(struct drm_panel *panel, bool enable)
{
	struct lcm *ctx;

	if (!panel) {
		pr_err("%s; panel is NULL\n", __func__);
		return;
	}

	ctx = panel_to_lcm(panel);
	ctx->dc_status = enable;
	gDcEnable = enable;

	pr_info("%s: dc backlight %s\n", __func__, enable ? "enabled" : "disabled");
}

static void panel_set_dc_threshold(struct drm_panel *panel, int dc_threshold)
{
	if (!panel) {
		pr_err("%s; panel is NULL\n", __func__);
		return;
	}

	gDcThreshold = dc_threshold;

	pr_info("%s: gDcThreshold = %d\n", __func__, gDcThreshold);
}

static int panel_fod_lhbm_init (struct mtk_dsi* dsi)
{
	if (!dsi) {
		pr_info("invalid dsi point\n");
		return -1;
	}
	pr_info("panel_fod_lhbm_init enter\n");
	dsi->display_type = "primary";
	dsi->mi_cfg.lhbm_ui_ready_delay_frame = 4;
	dsi->mi_cfg.local_hbm_enabled = 1;
	dsi->mi_cfg.fod_low_brightness_lux_threshold = 1;
	dsi->mi_cfg.fod_low_brightness_clone_threshold = 411;
	return 0;
}

static bool panel_get_dc(struct drm_panel *panel)
{
	struct lcm *ctx;

	if (!panel) {
		pr_err("%s; panel is NULL\n", __func__);
		return -1;
	}

	ctx = panel_to_lcm(panel);
	return ctx->dc_status;
}

static int panel_set_dc_crc(struct drm_panel *panel, int hw_brightness_evel,
	int crc_coef0, int crc_coef1)
{
	struct lcm *ctx;
	int ret = 0;
	int i = 0;
	int crc_level = 255;
	char crc_level_write_cmd0[] = {0xF0, 0x5A, 0x5A};
	char crc_level_write_cmd1[] = {0x1C, 0x03};
	char crc_level_write_cmd2[] = {0xB0, 0x00, 0x01, 0x1D};
	char crc_level_write_cmd3[] = {0x1D, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF};
	char crc_level_write_cmd4[] = {0x1D, 0x00};
	char crc_level_write_cmd5[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 6,
		.is_package = 1,
		.is_hs = 1,
	};

	pr_info("%s hw_brightness_evel = %d, crc_coef0 = %d, crc_coef1 = %d",
		__func__, hw_brightness_evel, crc_coef0, crc_coef1);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);
	if (!ctx->prepared) {
		pr_err("%s: panel is unprepare, return\n", __func__);
		ret = -1;
		goto err;
	}

	if (ctx->dc_status) {
		crc_level = (crc_coef0 * hw_brightness_evel + crc_coef1 + 5000) / 10000;
		if (crc_level > 255)
			crc_level = 255;
		if (crc_level < 1)
			crc_level = 1;
	} else {
		crc_level = 255.0;
	}
	pr_info("%s crc_level = %d", __func__, crc_level);
	if (ctx->crc_level == crc_level) {
		ret = 0;
		pr_info("%s crc_level not change, skip set crc", __func__);
		goto err;
	}

	for (i = 1; i < ARRAY_SIZE(crc_level_write_cmd3); i++) {
		if (crc_level_write_cmd3[i] != 0)
			crc_level_write_cmd3[i] = crc_level & 0xFF;
	}

	cmd_msg.type[0] = ARRAY_SIZE(crc_level_write_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = crc_level_write_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(crc_level_write_cmd0);

	cmd_msg.type[1] = ARRAY_SIZE(crc_level_write_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = crc_level_write_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(crc_level_write_cmd1);

	cmd_msg.type[2] = ARRAY_SIZE(crc_level_write_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[2] = crc_level_write_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(crc_level_write_cmd2);

	cmd_msg.type[3] = ARRAY_SIZE(crc_level_write_cmd3) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[3] = crc_level_write_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(crc_level_write_cmd3);

	cmd_msg.type[4] = ARRAY_SIZE(crc_level_write_cmd4) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[4] = crc_level_write_cmd4;
	cmd_msg.tx_len[4] = ARRAY_SIZE(crc_level_write_cmd4);

	cmd_msg.type[5] = ARRAY_SIZE(crc_level_write_cmd5) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[5] = crc_level_write_cmd5;
	cmd_msg.tx_len[5] = ARRAY_SIZE(crc_level_write_cmd5);

	ret = mtk_ddic_dsi_send_cmd_queue(&cmd_msg, true, true, true);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}

	ctx->crc_level = crc_level;

err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_restore_crc_level(struct drm_panel *panel, bool need_lock)
{
	struct lcm *ctx;
	int ret = 0;
	int i = 0;
	int crc_level = 255;
	char crc_level_write_cmd0[] = {0xF0, 0x5A, 0x5A};
	char crc_level_write_cmd1[] = {0x1C, 0x03};
	char crc_level_write_cmd2[] = {0xB0, 0x00, 0x01, 0x1D};
	char crc_level_write_cmd3[] = {0x1D, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF};
	char crc_level_write_cmd4[] = {0x1D, 0x00};
	char crc_level_write_cmd5[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 6,
                .is_package = 1,
                .is_hs = 1,
	};

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);

	crc_level = ctx->crc_level;
	if (crc_level == 0)
		crc_level = 255;

	pr_info("%s crc_level = %d", __func__, crc_level);

	for (i = 1; i < ARRAY_SIZE(crc_level_write_cmd3); i++) {
		if (crc_level_write_cmd3[i] != 0)
			crc_level_write_cmd3[i] = crc_level & 0xFF;
	}

	cmd_msg.type[0] = ARRAY_SIZE(crc_level_write_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = crc_level_write_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(crc_level_write_cmd0);

	cmd_msg.type[1] = ARRAY_SIZE(crc_level_write_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = crc_level_write_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(crc_level_write_cmd1);

	cmd_msg.type[2] = ARRAY_SIZE(crc_level_write_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[2] = crc_level_write_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(crc_level_write_cmd2);

	cmd_msg.type[3] = ARRAY_SIZE(crc_level_write_cmd3) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[3] = crc_level_write_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(crc_level_write_cmd3);

	cmd_msg.type[4] = ARRAY_SIZE(crc_level_write_cmd4) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[4] = crc_level_write_cmd4;
	cmd_msg.tx_len[4] = ARRAY_SIZE(crc_level_write_cmd4);

	cmd_msg.type[5] = ARRAY_SIZE(crc_level_write_cmd5) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[5] = crc_level_write_cmd5;
	cmd_msg.tx_len[5] = ARRAY_SIZE(crc_level_write_cmd5);

	ret = mtk_ddic_dsi_send_cmd_queue(&cmd_msg, true, true, need_lock);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}

err:
	return ret;
}

static int panel_set_dc_crc_off(struct drm_panel *panel)
{
	struct lcm *ctx;
	int ret = 0;
	char crc_level_write_cmd0[] = {0xF0, 0x5A, 0x5A};
	char crc_level_write_cmd1[] = {0x51, 0x03, 0xff};
	char crc_level_write_cmd2[] = {0x1C, 0x00};
	char crc_level_write_cmd3[] = {0x1D, 0x01};
	char crc_level_write_cmd4[] = {0xB0, 0x00, 0x01, 0x1D};
	char crc_level_write_cmd5[] = {0x1D, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF};
	char crc_level_write_cmd6[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 7,
	};

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);

	crc_level_write_cmd1[1] = bl_tb0[1];
	crc_level_write_cmd1[2] = bl_tb0[2];

	pr_info("%s 0x51 = 0x%02x, 0x%02x", __func__, crc_level_write_cmd1[1], crc_level_write_cmd1[2]);

	cmd_msg.type[0] = ARRAY_SIZE(crc_level_write_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = crc_level_write_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(crc_level_write_cmd0);

	cmd_msg.type[1] = ARRAY_SIZE(crc_level_write_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = crc_level_write_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(crc_level_write_cmd1);

	cmd_msg.type[2] = ARRAY_SIZE(crc_level_write_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[2] = crc_level_write_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(crc_level_write_cmd2);

	cmd_msg.type[3] = ARRAY_SIZE(crc_level_write_cmd3) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[3] = crc_level_write_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(crc_level_write_cmd3);

	cmd_msg.type[4] = ARRAY_SIZE(crc_level_write_cmd4) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[4] = crc_level_write_cmd4;
	cmd_msg.tx_len[4] = ARRAY_SIZE(crc_level_write_cmd4);

	cmd_msg.type[5] = ARRAY_SIZE(crc_level_write_cmd5) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[5] = crc_level_write_cmd5;
	cmd_msg.tx_len[5] = ARRAY_SIZE(crc_level_write_cmd5);

	cmd_msg.type[6] = ARRAY_SIZE(crc_level_write_cmd6) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[6] = crc_level_write_cmd6;
	cmd_msg.tx_len[6] = ARRAY_SIZE(crc_level_write_cmd6);

	ret = mtk_ddic_dsi_wait_te_send_cmd(&cmd_msg, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}

	ctx->crc_level = 0;

err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_set_gir_on(struct drm_panel *panel)
{
	struct lcm *ctx;
	int ret = 0;
	char gir_on_write_cmd0[] = {0xF0, 0x5A, 0x5A};
	char gir_on_write_cmd1[] = {0xB0, 0x02, 0xB5, 0x1D};
	char gir_on_write_cmd2[] = {0x1D, 0x27, 0x23, 0x6C, 0x03, 0x4E, 0x86, 0x01, 0xFF, 0x10, 0x73, 0xFF, 0x10, 0xFF, 0x6B, 0x8C, 0x2D, 0x06, 0x07,
		                        0x06, 0x1B, 0x1F, 0x18, 0x24, 0x29, 0x20, 0x2B, 0x31, 0x26, 0x2E, 0x34, 0x28, 0xA4, 0xE4, 0xA4, 0x08, 0x74,
		                        0x80, 0x00, 0x00, 0x22};
	char gir_on_write_cmd3[] = {0xF0, 0xA5, 0xA5};
	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 4,
	};

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
		goto err;
	}

	cmd_msg.type[0] = ARRAY_SIZE(gir_on_write_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = gir_on_write_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(gir_on_write_cmd0);

	cmd_msg.type[1] = ARRAY_SIZE(gir_on_write_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = gir_on_write_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(gir_on_write_cmd1);

	cmd_msg.type[2] = ARRAY_SIZE(gir_on_write_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[2] = gir_on_write_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(gir_on_write_cmd2);

	cmd_msg.type[3] = ARRAY_SIZE(gir_on_write_cmd3) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[3] = gir_on_write_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(gir_on_write_cmd3);

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, false, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_set_gir_off(struct drm_panel *panel)
{
	struct lcm *ctx;
	int ret = -1;
	char gir_off_write_cmd0[] = {0xF0, 0x5A, 0x5A};
	char gir_off_write_cmd1[] = {0xB0, 0x02, 0xB5, 0x1D};
	char gir_off_write_cmd2[] = {0x1D, 0x27, 0x03, 0xA4, 0x03, 0x5A, 0x80, 0x01, 0xFF, 0x10, 0x73, 0xFF, 0x10, 0xFF, 0x6B, 0x8C, 0x2D, 0x07, 0x07,
		                         0x07, 0x1E, 0x1E, 0x1E, 0x28, 0x28, 0x28, 0x2F, 0x2F, 0x2F, 0x32, 0x32, 0x32, 0xA4, 0xE4, 0xA4, 0x08, 0x74,
		                         0x80, 0x00, 0x00, 0x22};
	char gir_off_write_cmd3[] = {0xF0, 0xA5, 0xA5};
	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 4,
	};

	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}

	ctx = panel_to_lcm(panel);
	ctx->gir_status = 0;
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}

	cmd_msg.type[0] = ARRAY_SIZE(gir_off_write_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = gir_off_write_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(gir_off_write_cmd0);

	cmd_msg.type[1] = ARRAY_SIZE(gir_off_write_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = gir_off_write_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(gir_off_write_cmd1);

	cmd_msg.type[2] = ARRAY_SIZE(gir_off_write_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[2] = gir_off_write_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(gir_off_write_cmd2);

	cmd_msg.type[3] = ARRAY_SIZE(gir_off_write_cmd3) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[3] = gir_off_write_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(gir_off_write_cmd3);

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, false, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
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

static void panel_elvss_control(struct drm_panel *panel, bool en)
{
	struct lcm *ctx;

	int ret;
	char level2_key_enable_cmd0[] = {0xF0, 0x5A, 0x5A};
	char dimming_control_cmd1[] = {0x53, 0x20};
	char update_cmd2[] = {0xF7, 0x0F};
	char level2_key_disable_cmd3[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 4,
	};

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}

	pr_info("%s dimming = %d\n", __func__, en);
	ctx = panel_to_lcm(panel);

	if (en) {
		if (ctx->hbm_en)
			dimming_control_cmd1[1] = 0xE8;
		else
			dimming_control_cmd1[1] = 0x28;

	} else {
		if (ctx->hbm_en)
			dimming_control_cmd1[1] = 0xE0;
		else
			dimming_control_cmd1[1] = 0x20;
	}

	cmd_msg.type[0] = ARRAY_SIZE(level2_key_enable_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = level2_key_enable_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(level2_key_enable_cmd0);

	cmd_msg.type[1] = ARRAY_SIZE(dimming_control_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = dimming_control_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(dimming_control_cmd1);

	cmd_msg.type[2] = ARRAY_SIZE(update_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[2] = update_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(update_cmd2);

	cmd_msg.type[3] = ARRAY_SIZE(level2_key_disable_cmd3) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[3] = level2_key_disable_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(level2_key_disable_cmd3);

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, false, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
	pr_info("%s end -\n", __func__);
}

int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	/* Read 8 bytes from 0xA1 register
	 * 	BIT[0-1] = Wx
	 * 	BIT[2-3] = Wy */
	static uint16_t wx = 0, wy = 0;
	int ret = 0, count = 0;
	struct lcm *ctx = NULL;
	u8 tx_buf = 0xA1;
	u8 rx_buf[8] = {0x00};
	struct mtk_ddic_dsi_msg cmds = {
		.channel = 0,
		.tx_cmd_num = 1,
		.rx_cmd_num = 1,
		.type[0] = 0x06,
		.tx_buf[0] = &tx_buf,
		.tx_len[0] = 1,
		.rx_buf[0] = &rx_buf[0],
		.rx_len[0] = 8,
	};

	pr_info("%s: +\n", __func__);

	/* try to get wp info from cache */
	if (wx > 0 && wy > 0) {
		rx_buf[0] = (wx >> 8) & 0x00ff;
		rx_buf[1] = wx & 0x00ff;

		rx_buf[2] = (wy >> 8) & 0x00ff;
		rx_buf[3] = wy & 0x00ff;

		pr_info("%s: got wp info from cache\n", __func__);
		goto done;
	}

	/* try to get wp info from cmdline */
	if (sscanf(oled_wp_cmdline, "%02hhx%02hhx%02hhx%02hhx\n",
			&rx_buf[0], &rx_buf[1],
			&rx_buf[2], &rx_buf[3]) == 4) {

		if (rx_buf[0] == 1 && rx_buf[1] == 2 &&
			rx_buf[2] == 3 && rx_buf[3] == 4) {
			pr_err("No panel is Connected !");
			goto err;
		}

		wx = rx_buf[0] << 8 | rx_buf[1];
		wy = rx_buf[2] << 8 | rx_buf[3];
		if (wx > 0 && wy > 0) {
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
	ret |= mtk_ddic_dsi_read_cmd(&cmds);

	if (ret != 0) {
		pr_err("%s: failed to read ddic register\n", __func__);
		memset(rx_buf, 0, sizeof(rx_buf));
		goto err;
	}
	wx = rx_buf[0] << 8 | rx_buf[1];
	wy = rx_buf[2] << 8 | rx_buf[3];
done:
	count = snprintf(buf, size, "%02hhx%02hhx%02hhx%02hhx\n",
			rx_buf[0], rx_buf[1],
			rx_buf[2], rx_buf[3]);

	pr_info("%s: Wx=0x%02hx, Wy=0x%02hx\n", __func__, wx, wy);
err:
	pr_info("%s: -\n", __func__);
	return count;
}
#endif

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	char backlight_off_tb[] = {0x51, 0x00, 0x00};
	char level2_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char normal2aod_tb[] = {0x53, 0x24}; // aod mode
	char update_key_tb[] ={0xF7, 0x0F};
	char level2_key_disable_tb[] = {0xF0, 0xA5, 0xA5};

	atomic_set(&doze_enable, 1);
	pr_info("%s !+\n", __func__);

	cb(dsi, handle, backlight_off_tb, ARRAY_SIZE(backlight_off_tb));
	cb(dsi, handle, level2_key_enable_tb, ARRAY_SIZE(level2_key_enable_tb));
	cb(dsi, handle, normal2aod_tb, ARRAY_SIZE(normal2aod_tb));
	cb(dsi, handle, update_key_tb, ARRAY_SIZE(update_key_tb));
	cb(dsi, handle, level2_key_disable_tb, ARRAY_SIZE(level2_key_disable_tb));

	pr_info("%s !-\n", __func__);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	char aod2normal_cmd_start[] = {0xF0, 0x5A, 0x5A};
	char aod2normal_cmd1[] = {0x51, 0x00, 0x00};
	char aod2normal_cmd2[] = {0x53, 0x20};
	char aod2normal_cmd_end[] = {0xF0, 0xA5, 0xA5};
	struct lcm *ctx = panel_to_lcm(panel);

	pr_info("%s +\n", __func__);

	cb(dsi, handle, aod2normal_cmd_start, ARRAY_SIZE(aod2normal_cmd_start));
	cb(dsi, handle, aod2normal_cmd1, ARRAY_SIZE(aod2normal_cmd1));
	cb(dsi, handle, aod2normal_cmd2, ARRAY_SIZE(aod2normal_cmd2));
	cb(dsi, handle, aod2normal_cmd_end, ARRAY_SIZE(aod2normal_cmd_end));

	switch (ctx->dynamic_fps) {
		case 60:
			push_table(ctx, mode_60hz_setting,
				sizeof(mode_60hz_setting) / sizeof(struct LCM_setting_table));
			break;
		case 90:
			push_table(ctx, mode_90hz_setting,
				sizeof(mode_90hz_setting) / sizeof(struct LCM_setting_table));
			break;
		case 40:
			push_table(ctx, mode_40hz_setting,
				sizeof(mode_40hz_setting) / sizeof(struct LCM_setting_table));
			break;
		case 30:
			push_table(ctx, mode_30hz_setting,
				sizeof(mode_30hz_setting) / sizeof(struct LCM_setting_table));
			break;
		case 24:
			push_table(ctx, mode_24hz_setting,
				sizeof(mode_24hz_setting) / sizeof(struct LCM_setting_table));
			break;
		case 10:
			push_table(ctx, mode_10hz_setting,
				sizeof(mode_10hz_setting) / sizeof(struct LCM_setting_table));
			break;
		case 1:
			push_table(ctx, mode_1hz_setting,
				sizeof(mode_1hz_setting) / sizeof(struct LCM_setting_table));
			break;
		default: break;
	}
	atomic_set(&doze_enable, 0);

	ctx->doze_brightness_state = DOZE_TO_NORMAL;

	pr_info("%s -\n", __func__);

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.setbacklight_control = lcm_setbacklight_control,
	.panel_poweroff = lcm_panel_poweroff,
	.mode_switch = mode_switch,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
#ifdef CONFIG_MI_DISP
	.init = lcm_panel_init,
	.get_panel_info = panel_get_panel_info,
	.get_panel_initialized = get_lcm_initialized,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.hbm_fod_control = panel_hbm_fod_control,
	.set_lhbm_fod = panel_set_lhbm_fod,
	.panel_set_dc = panel_set_dc,
	.panel_get_dc = panel_get_dc,
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
	.get_panel_dynamic_fps =  panel_get_dynamic_fps,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
	.panel_elvss_control = panel_elvss_control,
	.get_wp_info = panel_get_wp_info,
	.panel_set_dc_crc = panel_set_dc_crc,
	.panel_restore_crc_level = panel_restore_crc_level,
	.panel_set_dc_crc_off = panel_set_dc_crc_off,
	.set_dc_backlight = panel_set_dc_backlight,
	.set_dc_threshold = panel_set_dc_threshold,
	.panel_fod_lhbm_init = panel_fod_lhbm_init,
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
	struct drm_display_mode *mode[16];
	int i;

	for (i = 0; i < 16; i++) {
		mode[i] = drm_mode_duplicate(connector->dev, &mode_config[i]);
		if (!mode[i]) {
			dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
				mode_config[i].hdisplay, mode_config[i].vdisplay,
				drm_mode_vrefresh(&mode_config[i]));
			return -ENOMEM;
		}
		drm_mode_set_name(mode[i]);
		mode[i]->type = DRM_MODE_TYPE_DRIVER;
		if (0 == i)
			mode[i]->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_probed_add(connector, mode[i]);
	}

	connector->display_info.width_mm = 71;
	connector->display_info.height_mm = 153;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void panel_config_init()
{
	int i;
	for (i = 0; i < MODE_FPS_NUM / 2; i++) {
		mode_config[i].hdisplay = HACT_WQHD;
		mode_config[i].hsync_start = HACT_WQHD + MODE1_HFP;
		mode_config[i].hsync_end = HACT_WQHD + MODE1_HFP + MODE1_HSA;
		mode_config[i].htotal = HACT_WQHD + MODE1_HFP + MODE1_HSA + MODE1_HBP;
		mode_config[i].vdisplay = VACT_WQHD;
		mode_config[i].vsync_start = VACT_WQHD + MODE1_VFP;
		mode_config[i].vsync_end = VACT_WQHD + MODE1_VFP + MODE1_VSA;
		mode_config[i].vtotal = VACT_WQHD + MODE1_VFP + MODE1_VSA + MODE1_VBP;
		mode_config[i].clock = mode_config[i].htotal * mode_config[i].vtotal * fps_mode[i] / 1000;
	}
	for (i = MODE_FPS_NUM / 2; i < MODE_FPS_NUM; i++) {
		mode_config[i].hdisplay = HACT_FHDP;
		mode_config[i].hsync_start = HACT_FHDP + MODE1_HFP;
		mode_config[i].hsync_end = HACT_FHDP + MODE1_HFP + MODE1_HSA;
		mode_config[i].htotal = HACT_FHDP + MODE1_HFP + MODE1_HSA + MODE1_HBP;
		mode_config[i].vdisplay = VACT_FHDP;
		mode_config[i].vsync_start = VACT_FHDP + MODE1_VFP;
		mode_config[i].vsync_end = VACT_FHDP + MODE1_VFP + MODE1_VSA;
		mode_config[i].vtotal = VACT_FHDP + MODE1_VFP + MODE1_VSA + MODE1_VBP;
		mode_config[i].clock = mode_config[i].htotal * mode_config[i].vtotal * fps_mode[i] / 1000;
	}
	ext_params_120hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_90hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_90hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_40hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_40hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_30hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_30hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_24hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_24hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_10hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_10hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_1hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_1hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_fhdp.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_fhdp.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_120hz_fhdp.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz_fhdp.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_90hz_fhdp.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_90hz_fhdp.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_40hz_fhdp.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_40hz_fhdp.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_30hz_fhdp.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_30hz_fhdp.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_24hz_fhdp.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_24hz_fhdp.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_10hz_fhdp.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_10hz_fhdp.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_1hz_fhdp.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_1hz_fhdp.err_flag_irq_flags = ext_params.err_flag_irq_flags;
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

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
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->dvdd_gpio = devm_gpiod_get_index(dev, "dvdd", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_gpio)) {
		dev_err(dev, "%s: cannot get dvdd_gpio 0 %ld\n",
			__func__, PTR_ERR(ctx->dvdd_gpio));
		return PTR_ERR(ctx->dvdd_gpio);
	}
	devm_gpiod_put(dev, ctx->dvdd_gpio);

	ret = lcm_panel_vci_regulator_init(dev);
	if (!ret)
		lcm_panel_vci_enable(dev);
	else
		pr_err("%s init vibr regulator error\n", __func__);

	ret = lcm_panel_vddi_regulator_init(dev);
	if (!ret)
		lcm_panel_vddi_enable(dev);
	else
		pr_err("%s init vaud18 regulator error\n", __func__);

	panel_config_init();

	ctx->prepared = true;
	ctx->enabled = true;
	ctx->panel_info = panel_name;
	ctx->dynamic_fps = 60;
	ctx->wqhd_en = true;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	ctx->dc_status = false;
	ctx->crc_level = 0;
	ctx->hbm_en = false;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

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

	pr_info("l2m_38_0a_0a_dsc_cmd %s-\n", __func__);

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
	{ .compatible = "l2m_38_0a_0a_dsc_cmd,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "l2m_38_0a_0a_dsc_cmd,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

module_param_string(oled_wp, oled_wp_cmdline, sizeof(oled_wp_cmdline), 0600);
MODULE_PARM_DESC(oled_wp, "oled_wp=<white_point_info>");

MODULE_AUTHOR("Yanjun Zhu <zhuyanjun1@xiaomi.com>");
MODULE_DESCRIPTION("L2M 38 0a 0a dsc wqhd oled panel driver");
MODULE_LICENSE("GPL v2");
