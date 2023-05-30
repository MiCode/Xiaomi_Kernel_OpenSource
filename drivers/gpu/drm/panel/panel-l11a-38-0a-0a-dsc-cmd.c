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
#include "../mediatek/mediatek_v2/mi_disp/mi_dsi_panel_count.h"
#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"
#include "mi_dsi_panel.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_wqhd_l11a_l.h"
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_wqhd_l11a_r.h"
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_fhd_l11a_l.h"
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_fhd_l11a_r.h"
#endif
#include <asm/atomic.h>

#define DATA_RATE		1360
#define MODE0_FPS		60
#define MODE1_FPS		120
#define MODE2_FPS		60
#define MODE3_FPS		120
#define MODE1_HFP		20
#define MODE1_HSA		2
#define MODE1_HBP		16
#define MODE1_VFP		54
#define MODE1_VSA		10
#define MODE1_VBP		10
#define HACT_WQHD		1440
#define VACT_WQHD		3200
#define HACT_FHDP		1080
#define VACT_FHDP		2400

#define MAX_BRIGHTNESS_CLONE 	16383
#define FPS_INIT_INDEX		11

static char bl_tb0[] = {0x51, 0x3, 0xff};
static const char *panel_name = "panel_name=dsi_l11a_38_0a_0a_dsc_cmd";
static atomic_t doze_enable = ATOMIC_INIT(0);
static int gDcThreshold = 450;
static bool gDcEnable = false;
static char oled_wp_cmdline[16] = {0};
static u8 id1 = 0;

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

static int  lcm_get_dcs_panel_id(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret = 0;

	if (!ctx) {
		pr_err("ctx is null\n");
		return ret;
	}

	if (ret == 0 && id1 == 0) {
		ret = lcm_dcs_read(ctx,  0xDA, buffer, 1);
		id1 = buffer[0] | (buffer[1] << 8);
		pr_info("lcm get dcs panel_id, ret:%d id1:0x%x \n", ret, id1);
	}
	return ret;
}

#ifdef PANEL_SUPPORT_READBACK
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
//AMOLED 1.8V:VDDI_1P8,VRF18_AIF
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

#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF

static struct LCM_setting_table init_setting_fhdp[] = {
	/* FHDP DSC setting */
	{0x9D, 01, {0x01}},
	{0x9E, 120, {0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
			0x04, 0x38, 0x00, 0x20, 0x02, 0x1C, 0x02, 0x1C,
			0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0x15,
			0x00, 0x07, 0x00, 0x0C, 0x03, 0x19, 0x03, 0x2E,
			0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
			0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
			0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
			0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {} },
	/* common setting */
	{0x35, 01, {0x00}},
	/* page address set */
	{0x2A, 04, {0x00, 0x00, 0x05, 0x9F}},
	{0x2B, 04, {0x00, 0x00, 0x0c, 0x7F}},
	/* scaler setting FHDP */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 24, {0x4B, 0x00, 0x00, 0x00, 0xFF, 0xF5, 0xF7, 0xFA,
			0xFE, 0x00, 0x03, 0x09, 0x10, 0x18, 0x00, 0x21,
			0x2C, 0x37, 0x43, 0x00, 0x50, 0x5D, 0x6B, 0x79}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* frequency select 60hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x08}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* VLIN1 set 7.6V */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0A, 0xB1}},
	{0xB1, 01, {0x38}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* FD Setting*/
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0B, 0xB1}},
	{0xB1, 01, {0x40}},
	{0xF1, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* dimming setting */
	{0xF0, 02, {0x5A, 0x5A}},
	/* dimming speed setting */
	{0xB0, 03, {0x00, 0x0E, 0x94}},
	{0x94, 01, {0x08}},
	/* elvss dim setting */
	{0xB0, 03, {0x00, 0x0D, 0x94}},
	{0x94, 01, {0x60}},
	{0x53, 01, {0x28}},
	{0x51, 02, {0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* FFC setting */
	{0xFC, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x2A, 0xC5}},
	{0xC5, 28, {0x11, 0x10, 0x50, 0x05, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C}},
	{0xFC, 02, {0xA5, 0xA5}},

	/* Err_flag setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x4C, 0xF4}},
	{0xF4, 01, {0xF0}},
	{0xE5, 01, {0x15}},
	{0xED, 03, {0x45, 0x4D, 0x60}},
	{0xF0, 02, {0xA5, 0xA5}},

	{0x29, 01, {0x00}},

	/* TE fixed */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x01, 0xBD}},
	{0xBD, 01, {0x83}},
	{0xB0, 03, {0x00, 0x2D, 0xBD}},
	{0xBD, 01, {0x04}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_wqhd[] = {
	/* WQHD DSC setting */
	{0x9D, 01, {0x01}},
	{0x9E, 120, {0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x80,
			0x05, 0xA0, 0x00, 0x19, 0x02, 0xD0, 0x02, 0xD0,
			0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x02, 0xBE,
			0x00, 0x0A, 0x00, 0x0C, 0x04, 0x00, 0x03, 0x0D,
			0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
			0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
			0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
			0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	{0x11, 01, {0x00}},
	{REGFLAG_DELAY, 120, {} },
	/* common setting */
	{0x35, 01, {0x00}},
	/* page address set */
	{0x2A, 04, {0x00, 0x00, 0x05, 0x9F}},
	{0x2B, 04, {0x00, 0x00, 0x0c, 0x7F}},
	/* scaler setting WQHD */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x4C}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* frequency select 60hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x08}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* VLIN1 set 7.6V */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0A, 0xB1}},
	{0xB1, 01, {0x38}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* FD Setting*/
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x0B, 0xB1}},
	{0xB1, 01, {0x40}},
	{0xF1, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* dimming setting */
	{0xF0, 02, {0x5A, 0x5A}},
	/* dimming speed setting */
	{0xB0, 03, {0x00, 0x0E, 0x94}},
	{0x94, 01, {0x08}},
	/* elvss dim setting */
	{0xB0, 03, {0x00, 0x0D, 0x94}},
	{0x94, 01, {0x60}},
	{0x53, 01, {0x28}},
	{0x51, 02, {0x00, 0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},

	/* FFC setting */
	{0xFC, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x2A, 0xC5}},
	{0xC5, 28, {0x11, 0x10, 0x50, 0x05, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C, 0x47, 0x4B, 0x46, 0x5A,
			0x4D, 0x9A, 0x4C, 0x0C}},
	{0xFC, 02, {0xA5, 0xA5}},

	/* Err_flag setting */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x4C, 0xF4}},
	{0xF4, 01, {0xF0}},
	{0xE5, 01, {0x15}},
	{0xED, 03, {0x45, 0x4D, 0x60}},
	{0xF0, 02, {0xA5, 0xA5}},

	{0x29, 01, {0x00}},

	/* TE fixed */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x01, 0xBD}},
	{0xBD, 01, {0x83}},
	{0xB0, 03, {0x00, 0x2D, 0xBD}},
	{0xBD, 01, {0x04}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_gir_on[] = {
	{0xF0, 0x02, {0x5A, 0x5A}},
	{0xB0, 0x03, {0x01, 0x52, 0x92}},
	{0x92, 0x01, {0x21}},
	{0xF7, 0x01, {0x0F}},
	{0xF0, 0x02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_gir_off[] = {
	{0xF0, 0x02, {0x5A, 0x5A}},
	{0xB0, 0x03, {0x01, 0x52, 0x92}},
	{0x92, 0x01, {0x01}},
	{0xF7, 0x01, {0x0F}},
	{0xF0, 0x02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_spr_1d[] = {
	{0xF0, 0x02, {0x5A, 0x5A}},
	{0xB0, 0x03, {0x01, 0x1D, 0x92}},
	{0x92, 0x01, {0x71}},
	{0xF0, 0x02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table init_setting_spr_2d[] = {
	{0xF0, 0x02, {0x5A, 0x5A}},
	{0xB0, 0x03, {0x01, 0x1D, 0x92}},
	{0x92, 0x01, {0x70}},
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

#ifdef CONFIG_MI_DISP_PANEL_COUNT
	/* add for display fps count*/
	dsi_panel_fps_count(ctx, 0, 0);
	/* add for display state count */
	if (ctx->mi_count.panel_active_count_enable) {
		dsi_panel_state_count(ctx, 0);
	}
#endif

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

	udelay(2000);
	lcm_panel_vci_disable(ctx->dev);
	udelay(2000);
	lcm_panel_vddi_disable(ctx->dev);
	udelay(2000);
	atomic_set(&doze_enable, 0);
	return 0;
}

static int  lcm_get_panel_id(struct lcm *ctx)
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

	pr_info(" %s id=0x%x +\n", __func__,id1);

	if (id1)
		return ret;

	if (id1 == 0) {
		ret = mtk_ddic_dsi_read_cmd(&cmds[0]);
		id1 = rx_buf[0];
	}

	pr_info(" %s panel_id=0x%x -\n", __func__,id1);
	return ret;
}

static bool lcm_panel_is_p0()
{
	if (id1 == 0x01 || id1 == 0x12 || id1 == 0x43 || id1 ==0x54 || id1 ==0x85)
		return true;
	else
		return false;
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

#ifdef CONFIG_MI_DISP_PANEL_COUNT
	/* add  for display fps count */
	dsi_panel_fps_count(ctx, 0, 1);
	/* add for display state count */
	if (ctx->mi_count.panel_active_count_enable) {
		dsi_panel_state_count(ctx, 0);
	}
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

static const struct drm_display_mode default_mode = {
	.clock = 288750,
	.hdisplay = HACT_WQHD,
	.hsync_start = HACT_WQHD + MODE1_HFP,
	.hsync_end = HACT_WQHD + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_WQHD + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_WQHD,
	.vsync_start = VACT_WQHD + MODE1_VFP,
	.vsync_end = VACT_WQHD + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_WQHD + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

static const struct drm_display_mode performence_mode = {
	.clock = 582000,
	.hdisplay = HACT_WQHD,
	.hsync_start = HACT_WQHD + MODE1_HFP,
	.hsync_end = HACT_WQHD + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_WQHD + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_WQHD,
	.vsync_start = VACT_WQHD + MODE1_VFP,
	.vsync_end = VACT_WQHD + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_WQHD + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

static const struct drm_display_mode default_mode_fhdp = {
	.clock = 166300,
	.hdisplay = HACT_FHDP,
	.hsync_start = HACT_FHDP + MODE1_HFP,
	.hsync_end = HACT_FHDP + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_FHDP + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_FHDP,
	.vsync_start = VACT_FHDP + MODE1_VFP,
	.vsync_end = VACT_FHDP + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_FHDP + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

static const struct drm_display_mode performence_mode_fhdp = {
	.clock = 332600,
	.hdisplay = HACT_FHDP,
	.hsync_start = HACT_FHDP + MODE1_HFP,
	.hsync_end = HACT_FHDP + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_FHDP + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_FHDP,
	.vsync_start = VACT_FHDP + MODE1_VFP,
	.vsync_end = VACT_FHDP + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_FHDP + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = 680,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69552,
	.physical_height_um = 154560,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 1300,
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
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 702,
		.decrement_interval = 10,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1024,
		.slice_bpg_offset = 781,
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
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69552,
	.physical_height_um = 154560,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 90,
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
		.pic_height = 3200,
		.pic_width = 1440,
		.slice_height = 25,
		.slice_width = 720,
		.chunk_size = 720,
		.xmit_delay = 512,
		.dec_delay = 616,
		.scale_value = 32,
		.increment_interval = 702,
		.decrement_interval = 10,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 1024,
		.slice_bpg_offset = 781,
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
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69552,
	.physical_height_um = 154560,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 962,
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
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 789,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 793,
		.slice_bpg_offset = 814,
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
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69552,
	.physical_height_um = 154560,
	.cmd_null_pkt_en = 1,
	.cmd_null_pkt_len = 962,
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
		.slice_height = 32,
		.slice_width = 540,
		.chunk_size = 540,
		.xmit_delay = 512,
		.dec_delay = 526,
		.scale_value = 32,
		.increment_interval = 789,
		.decrement_interval = 7,
		.line_bpg_offset = 12,
		.nfl_bpg_offset = 793,
		.slice_bpg_offset = 814,
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
		*ext_param = &ext_params_120hz_fhdp;
	else if (mode == 3)
		*ext_param = &ext_params_fhdp;
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

	if (drm_mode_vrefresh(m) == MODE0_FPS && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == MODE1_FPS && m->hdisplay == HACT_WQHD)
		ext->params = &ext_params_120hz;
	else if (drm_mode_vrefresh(m) == MODE2_FPS && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_fhdp;
	else if (drm_mode_vrefresh(m) == MODE3_FPS && m->hdisplay == HACT_FHDP)
		ext->params = &ext_params_120hz_fhdp;
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

	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;
	struct drm_panel *panel = mtk_dsi->panel;
	struct lcm *ctx;

	if (!panel) {
		pr_err(": panel is NULL\n", __func__);
		return -1;
	}
	ctx = panel_to_lcm(panel);

	if (level) {
		bl_tb0[1] = (level >> 8) & 0xFF;
		bl_tb0[2] = level & 0xFF;
	}

	if (gDcEnable && level && level < gDcThreshold)
		level = gDcThreshold;

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

static struct LCM_setting_table mode_120hz_setting[] = {
	/* frequency select 120hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x00}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_60hz_setting[] = {
	/* frequency select 60hz */
	{0xF0, 02, {0x5A, 0x5A}},
	{0x60, 01, {0x08}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_wqhd_setting[] = {
	/* WQHD DSC setting */
	{0x9D, 01, {0x01}},
	{0x9E, 120, {0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0C, 0x80,
			0x05, 0xA0, 0x00, 0x19, 0x02, 0xD0, 0x02, 0xD0,
			0x02, 0x00, 0x02, 0x68, 0x00, 0x20, 0x02, 0xBE,
			0x00, 0x0A, 0x00, 0x0C, 0x04, 0x00, 0x03, 0x0D,
			0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
			0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
			0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
			0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	/* scaler setting WQHD */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 01, {0x4C}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* TE fixed */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x01, 0xBD}},
	{0xBD, 01, {0x83}},
	{0xB0, 03, {0x00, 0x2D, 0xBD}},
	{0xBD, 01, {0x04}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_fhdp_setting[] = {
	/* FHDP DSC setting */
	{0x9D, 01, {0x01}},
	{0x9E, 120, {0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x09, 0x60,
			0x04, 0x38, 0x00, 0x20, 0x02, 0x1C, 0x02, 0x1C,
			0x02, 0x00, 0x02, 0x0E, 0x00, 0x20, 0x03, 0x15,
			0x00, 0x07, 0x00, 0x0C, 0x03, 0x19, 0x03, 0x2E,
			0x18, 0x00, 0x10, 0xF0, 0x03, 0x0C, 0x20, 0x00,
			0x06, 0x0B, 0x0B, 0x33, 0x0E, 0x1C, 0x2A, 0x38,
			0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7B,
			0x7D, 0x7E, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
			0x09, 0xBE, 0x19, 0xFC, 0x19, 0xFA, 0x19, 0xF8,
			0x1A, 0x38, 0x1A, 0x78, 0x1A, 0xB6, 0x2A, 0xF6,
			0x2B, 0x34, 0x2B, 0x74, 0x3B, 0x74, 0x6B, 0xF4,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
	/* scaler setting FHDP */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xC3, 24, {0x4B, 0x00, 0x00, 0x00, 0xFF, 0xF5, 0xF7, 0xFA,
			0xFE, 0x00, 0x03, 0x09, 0x10, 0x18, 0x00, 0x21,
			0x2C, 0x37, 0x43, 0x00, 0x50, 0x5D, 0x6B, 0x79}},
	{0xF0, 02, {0xA5, 0xA5}},
	/* TE fixed */
	{0xF0, 02, {0x5A, 0x5A}},
	{0xB0, 03, {0x00, 0x01, 0xBD}},
	{0xBD, 01, {0x83}},
	{0xB0, 03, {0x00, 0x2D, 0xBD}},
	{0xBD, 01, {0x04}},
	{0xF7, 01, {0x0F}},
	{0xF0, 02, {0xA5, 0xA5}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		push_table(ctx, mode_120hz_setting,
			sizeof(mode_120hz_setting) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 120;
	}
#ifdef CONFIG_MI_DISP_PANEL_COUNT
	/* add  for display fps count */
	dsi_panel_fps_count(ctx, 120, 1);
#endif
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		push_table(ctx, mode_60hz_setting,
			sizeof(mode_60hz_setting) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 60;
	}
#ifdef CONFIG_MI_DISP_PANEL_COUNT
	/* add  for display fps count */
	dsi_panel_fps_count(ctx, 60, 1);
#endif
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
		if (drm_mode_vrefresh(m_dst) == MODE0_FPS)
			mode_switch_to_60(panel, stage);
		else if (drm_mode_vrefresh(m_dst) == MODE1_FPS)
			mode_switch_to_120(panel, stage);
		else
			ret1 = 1;
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

	pr_info("%s: +\n", __func__);
	if (ctx->prepared) {
		pr_info("%s: panel has been prepared, nothing to do!\n", __func__);
		goto err;
	}

	if (ctx->dynamic_fps == 120) {
		if (init_setting_wqhd[FPS_INIT_INDEX].cmd == 0x60)
			init_setting_wqhd[FPS_INIT_INDEX].para_list[0] = 0x00;
		if (init_setting_fhdp[FPS_INIT_INDEX].cmd == 0x60)
			init_setting_fhdp[FPS_INIT_INDEX].para_list[0] = 0x00;
	} else if (ctx->dynamic_fps == 60) {
		if (init_setting_wqhd[FPS_INIT_INDEX].cmd == 0x60)
			init_setting_wqhd[FPS_INIT_INDEX].para_list[0] = 0x08;
		if (init_setting_fhdp[FPS_INIT_INDEX].cmd == 0x60)
			init_setting_fhdp[FPS_INIT_INDEX].para_list[0] = 0x08;
	}

	if (ctx->wqhd_en)
		push_table(ctx, init_setting_wqhd, sizeof(init_setting_wqhd) / sizeof(struct LCM_setting_table));
	else
		push_table(ctx, init_setting_fhdp, sizeof(init_setting_fhdp) / sizeof(struct LCM_setting_table));

	if (ctx->gir_status) {
		push_table(ctx, init_setting_gir_on,
				sizeof(init_setting_gir_on) / sizeof(struct LCM_setting_table));
	} else {
		push_table(ctx, init_setting_gir_off,
				sizeof(init_setting_gir_off) / sizeof(struct LCM_setting_table));
	}

	switch (ctx->spr_status) {
		case SPR_1D_RENDERING:
			push_table(ctx, init_setting_spr_1d,
					sizeof(init_setting_spr_1d) / sizeof(struct LCM_setting_table));
			break;
		case SPR_2D_RENDERING:
			push_table(ctx, init_setting_spr_2d,
					sizeof(init_setting_spr_2d) / sizeof(struct LCM_setting_table));
			break;
		default:
			break;
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
}

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
	struct lcm *ctx;

	int ret;
	char level2_key_enable_cmd0[] = {0xF0, 0x5A, 0x5A};
	char aod_hbm_control[] = {0x53, 0x24};
	char update_cmd2[] = {0xF7, 0x0F};
	char level2_key_disable_cmd3[] = {0xF0, 0xA5, 0xA5};
	char backlight_h[] = {0x51, 0x07, 0xFF};
	char backlight_l[] = {0x51, 0x00, 0xAA};
	char backlight_ll[] = {0x51, 0x00, 0x80};
	char aod2normal_tb[] = {0x53, 0x28};
	char display_on_tb[] = {0x29, 0x00};

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

	lcm_get_panel_id(ctx);

	pr_info("%s+ doze_brightness:%d\n", __func__, doze_brightness);

	cmd_msg.type[0] = ARRAY_SIZE(level2_key_enable_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = level2_key_enable_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(level2_key_enable_cmd0);

	cmd_msg.type[1] = ARRAY_SIZE(aod_hbm_control) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = aod_hbm_control;
	cmd_msg.tx_len[1] = ARRAY_SIZE(aod_hbm_control);

	if (DOZE_TO_NORMAL == doze_brightness) {
		atomic_set(&doze_enable, 0);
		goto exit;
	}

	if (!lcm_panel_is_p0()) {
		cmd_msg.type[2] = ARRAY_SIZE(backlight_ll) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[2] = backlight_ll;
		cmd_msg.tx_len[2] = ARRAY_SIZE(backlight_ll);

		cmd_msg.type[3] = ARRAY_SIZE(update_cmd2) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[3] = update_cmd2;
		cmd_msg.tx_len[3] = ARRAY_SIZE(update_cmd2);

		cmd_msg.type[4] = ARRAY_SIZE(level2_key_disable_cmd3) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[4] = level2_key_disable_cmd3;
		cmd_msg.tx_len[4] = ARRAY_SIZE(level2_key_disable_cmd3);

		if (atomic_read(&doze_enable) == 1)
			atomic_set(&doze_enable, 2);

		cmd_msg.tx_cmd_num = 5;

	} else {
		aod_hbm_control[1] = 0x25;

		cmd_msg.type[2] = ARRAY_SIZE(update_cmd2) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[2] = update_cmd2;
		cmd_msg.tx_len[2] = ARRAY_SIZE(update_cmd2);

		cmd_msg.type[3] = ARRAY_SIZE(level2_key_disable_cmd3) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[3] = level2_key_disable_cmd3;
		cmd_msg.tx_len[3] = ARRAY_SIZE(level2_key_disable_cmd3);

		if (atomic_read(&doze_enable) == 1) {
			cmd_msg.type[4] = ARRAY_SIZE(display_on_tb) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[4] = display_on_tb;
			cmd_msg.tx_len[4] = ARRAY_SIZE(display_on_tb);
			cmd_msg.tx_cmd_num = 5;
			atomic_set(&doze_enable, 2);
		} else {
			cmd_msg.tx_cmd_num = 4;
		}
	}

	if(atomic_read(&doze_enable) == 0) {
		pr_info("%s doze_brightness_state is DOZE_TO_NORMAL\n",__func__);
		goto exit;
	}

	if(atomic_read(&doze_enable) == 2) {
		msleep(80);
	}

	pr_info("%s+ new doze_brightness:%d doze_enable:%d,\n", __func__, doze_brightness, atomic_read(&doze_enable));
	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}

	if (!lcm_panel_is_p0()) {
		if (doze_brightness == DOZE_BRIGHTNESS_HBM) {
			cmd_msg.type[0] = ARRAY_SIZE(backlight_h) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[0] = backlight_h;
			cmd_msg.tx_len[0] = ARRAY_SIZE(backlight_h);
		} else {
			cmd_msg.type[0] = ARRAY_SIZE(backlight_l) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[0] = backlight_l;
			cmd_msg.tx_len[0] = ARRAY_SIZE(backlight_l);
		}
		cmd_msg.tx_cmd_num = 1;

		msleep(80);
		ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
		if (ret != 0) {
			DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
		}
	} else {
		if (doze_brightness == DOZE_BRIGHTNESS_HBM) {
			cmd_msg.type[0] = ARRAY_SIZE(level2_key_enable_cmd0) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[0] = level2_key_enable_cmd0;
			cmd_msg.tx_len[0] = ARRAY_SIZE(level2_key_enable_cmd0);

			aod_hbm_control[1] = 0x24;
			cmd_msg.type[1] = ARRAY_SIZE(aod_hbm_control) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[1] = aod_hbm_control;
			cmd_msg.tx_len[1] = ARRAY_SIZE(aod_hbm_control);

			cmd_msg.type[2] = ARRAY_SIZE(update_cmd2) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[2] = update_cmd2;
			cmd_msg.tx_len[2] = ARRAY_SIZE(update_cmd2);

			cmd_msg.type[3] = ARRAY_SIZE(level2_key_disable_cmd3) > 2 ? 0x39 : 0x15;
			cmd_msg.tx_buf[3] = level2_key_disable_cmd3;
			cmd_msg.tx_len[3] = ARRAY_SIZE(level2_key_disable_cmd3);

			cmd_msg.tx_cmd_num = 4;

			msleep(80);
			ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
			if (ret != 0) {
				DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
			}
		}
	}

	if(atomic_read(&doze_enable) == 0) {
		cmd_msg.type[0] = ARRAY_SIZE(level2_key_enable_cmd0) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[0] = level2_key_enable_cmd0;
		cmd_msg.tx_len[0] = ARRAY_SIZE(level2_key_enable_cmd0);

		cmd_msg.type[1] = ARRAY_SIZE(aod2normal_tb) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[1] = aod2normal_tb;
		cmd_msg.tx_len[1] = ARRAY_SIZE(aod2normal_tb);
		backlight_l[1] = 0;
		backlight_l[2] = 0;
		cmd_msg.type[2] = ARRAY_SIZE(backlight_l) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[2] = backlight_l;
		cmd_msg.tx_len[2] = ARRAY_SIZE(backlight_l);

		cmd_msg.type[3] = ARRAY_SIZE(update_cmd2) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[3] = update_cmd2;
		cmd_msg.tx_len[3] = ARRAY_SIZE(update_cmd2);

		cmd_msg.type[4] = ARRAY_SIZE(level2_key_disable_cmd3) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[4] = level2_key_disable_cmd3;
		cmd_msg.tx_len[4] = ARRAY_SIZE(level2_key_disable_cmd3);
		cmd_msg.tx_cmd_num = 5;
		ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
		if (ret != 0) {
			DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
		}
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
	char crc_level_write_cmd1[] = {0x92, 0x83};
	char crc_level_write_cmd2[] = {0xB0, 0x00, 0x01, 0x92};
	char crc_level_write_cmd3[] = {0x92, 0x00};
	char crc_level_write_cmd4[] = {0xB0, 0x00, 0x2C, 0x92};
	char crc_level_write_cmd5[] = {0x92, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF};
	char crc_level_write_cmd6[] = {0xB0, 0x00, 0x56, 0x92};
	char crc_level_write_cmd7[] = {0x92, 0x16};
	char crc_level_write_cmd8[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 9,
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

	for (i = 1; i < ARRAY_SIZE(crc_level_write_cmd5); i++) {
		if (crc_level_write_cmd5[i] != 0)
			crc_level_write_cmd5[i] = crc_level & 0xFF;
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

	cmd_msg.type[6] = ARRAY_SIZE(crc_level_write_cmd6) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[6] = crc_level_write_cmd6;
	cmd_msg.tx_len[6] = ARRAY_SIZE(crc_level_write_cmd6);

	cmd_msg.type[7] = ARRAY_SIZE(crc_level_write_cmd7) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[7] = crc_level_write_cmd7;
	cmd_msg.tx_len[7] = ARRAY_SIZE(crc_level_write_cmd7);

	cmd_msg.type[8] = ARRAY_SIZE(crc_level_write_cmd8) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[8] = crc_level_write_cmd8;
	cmd_msg.tx_len[8] = ARRAY_SIZE(crc_level_write_cmd8);

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, true);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}

	ctx->crc_level = crc_level;

err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_set_dc_crc_bl_pack(struct drm_panel *panel, int hw_brightness_evel,
	int crc_coef0, int crc_coef1)
{
	struct lcm *ctx;
	int ret = 0;
	int i = 0;
	int crc_level = 255;
	char crc_level_write_cmd0[] = {0xF0, 0x5A, 0x5A};
	char crc_level_write_cmd1[] = {0x92, 0x83};
	char crc_level_write_cmd2[] = {0xB0, 0x00, 0x01, 0x92};
	char crc_level_write_cmd3[] = {0x92, 0x00};
	char crc_level_write_cmd4[] = {0xB0, 0x00, 0x2C, 0x92};
	char crc_level_write_cmd5[] = {0x92, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF};
	char crc_level_write_cmd6[] = {0xB0, 0x00, 0x56, 0x92};
	char crc_level_write_cmd7[] = {0x92, 0x16};
	char crc_level_write_cmd8[] = {0xF0, 0xA5, 0xA5};
	char crc_level_write_cmd9[] = {0x51, 0x00, 0x00};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 1,
		.tx_cmd_num = 10,
	};

	pr_info("%s hw_brightness_evel = %d, crc_coef0 = %d, crc_coef1 = %d",
		__func__, hw_brightness_evel, crc_coef0, crc_coef1);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);

	if (ctx->dc_status) {
		crc_level = (crc_coef0 * hw_brightness_evel + crc_coef1 + 5000) / 10000;
		if (crc_level > 255)
			crc_level = 255;
		if (crc_level < 1)
			crc_level = 1;
		crc_level_write_cmd9[1] = (gDcThreshold >> 8) & 0xFF;
		crc_level_write_cmd9[2] = gDcThreshold & 0xFF;
	} else {
		crc_level = 255.0;
	}
	pr_info("%s crc_level = %d", __func__, crc_level);

	for (i = 1; i < ARRAY_SIZE(crc_level_write_cmd5); i++) {
		if (crc_level_write_cmd5[i] != 0)
			crc_level_write_cmd5[i] = crc_level & 0xFF;
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

	cmd_msg.type[6] = ARRAY_SIZE(crc_level_write_cmd6) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[6] = crc_level_write_cmd6;
	cmd_msg.tx_len[6] = ARRAY_SIZE(crc_level_write_cmd6);

	cmd_msg.type[7] = ARRAY_SIZE(crc_level_write_cmd7) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[7] = crc_level_write_cmd7;
	cmd_msg.tx_len[7] = ARRAY_SIZE(crc_level_write_cmd7);

	cmd_msg.type[8] = ARRAY_SIZE(crc_level_write_cmd8) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[8] = crc_level_write_cmd8;
	cmd_msg.tx_len[8] = ARRAY_SIZE(crc_level_write_cmd8);

	cmd_msg.type[9] = ARRAY_SIZE(crc_level_write_cmd9) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[9] = crc_level_write_cmd9;
	cmd_msg.tx_len[9] = ARRAY_SIZE(crc_level_write_cmd9);

	ret = mtk_ddic_dsi_wait_te_send_cmd(&cmd_msg, true);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}

	//ctx->crc_level = crc_level;

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
	char crc_level_write_cmd1[] = {0x92, 0x83};
	char crc_level_write_cmd2[] = {0xB0, 0x00, 0x01, 0x92};
	char crc_level_write_cmd3[] = {0x92, 0x00};
	char crc_level_write_cmd4[] = {0xB0, 0x00, 0x2C, 0x92};
	char crc_level_write_cmd5[] = {0x92, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF};
	char crc_level_write_cmd6[] = {0xB0, 0x00, 0x56, 0x92};
	char crc_level_write_cmd7[] = {0x92, 0x16};
	char crc_level_write_cmd8[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 9,
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

	for (i = 1; i < ARRAY_SIZE(crc_level_write_cmd5); i++) {
		if (crc_level_write_cmd5[i] != 0)
			crc_level_write_cmd5[i] = crc_level & 0xFF;
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

	cmd_msg.type[6] = ARRAY_SIZE(crc_level_write_cmd6) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[6] = crc_level_write_cmd6;
	cmd_msg.tx_len[6] = ARRAY_SIZE(crc_level_write_cmd6);

	cmd_msg.type[7] = ARRAY_SIZE(crc_level_write_cmd7) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[7] = crc_level_write_cmd7;
	cmd_msg.tx_len[7] = ARRAY_SIZE(crc_level_write_cmd7);

	cmd_msg.type[8] = ARRAY_SIZE(crc_level_write_cmd8) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[8] = crc_level_write_cmd8;
	cmd_msg.tx_len[8] = ARRAY_SIZE(crc_level_write_cmd8);

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
	char crc_level_write_cmd1[] = {0x92, 0x00};
	char crc_level_write_cmd2[] = {0xB0, 0x00, 0x01, 0x92};
	char crc_level_write_cmd3[] = {0x92, 0x01};
	char crc_level_write_cmd4[] = {0xF0, 0xA5, 0xA5};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 5,
	};

	pr_info("%s ", __func__);
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);

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

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
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
	char gir_on_write_cmd1[] = {0xB0, 0x01, 0x52, 0x92};
	char gir_on_write_cmd2[] = {0x92, 0x21};
	char gir_on_write_cmd3[] = {0xF7, 0x0F};
	char gir_on_write_cmd4[] = {0xF0, 0xA5, 0xA5};
	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 5,
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

	cmd_msg.type[4] = ARRAY_SIZE(gir_on_write_cmd4) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[4] = gir_on_write_cmd4;
	cmd_msg.tx_len[4] = ARRAY_SIZE(gir_on_write_cmd4);

	ret = mtk_ddic_dsi_wait_te_send_cmd(&cmd_msg, true);
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
	char gir_off_write_cmd1[] = {0xB0, 0x01, 0x52, 0x92};
	char gir_off_write_cmd2[] = {0x92, 0x01};
	char gir_off_write_cmd3[] = {0xF7, 0x0F};
	char gir_off_write_cmd4[] = {0xF0, 0xA5, 0xA5};
	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 5,
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

	cmd_msg.type[4] = ARRAY_SIZE(gir_off_write_cmd4) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[4] = gir_off_write_cmd4;
	cmd_msg.tx_len[4] = ARRAY_SIZE(gir_off_write_cmd4);

	ret = mtk_ddic_dsi_wait_te_send_cmd(&cmd_msg, false);
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

static void lcm_esd_restore_backlight(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));

	pr_info("%s high_bl = 0x%x, low_bl = 0x%x \n", __func__, bl_tb0[1], bl_tb0[2]);

	return;
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

	if (en)
		dimming_control_cmd1[1] = 0x28;
	else
		dimming_control_cmd1[1] = 0x20;

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

	ret = mtk_ddic_dsi_wait_te_send_cmd(&cmd_msg, true);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
	pr_info("%s end -\n", __func__);
}

int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	/* Read 16 bytes from 0xA1 register
	 * 	BIT[0-1] = Wx
	 * 	BIT[2-3] = Wy
	 * 	BIT[13-14] = Lux */
	static uint16_t lux = 0, wx = 0, wy = 0;
	int i, ret = 0, count = 0;
	struct lcm *ctx = NULL;
	u8 tx_buf[] = {0xA1, 0xA8};
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
			.rx_len[0] = 8,
		},
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &tx_buf[1],
			.tx_len[0] = 1,
			.rx_buf[0] = &rx_buf[8],
			.rx_len[0] = 8,
		}
	};

	pr_info("%s: +\n", __func__);

	/* try to get wp info from cache */
	if (lux > 0 && wx > 0 && wy > 0) {
		rx_buf[13]  = (lux >> 8) & 0x00ff;
		rx_buf[14] = lux & 0x00ff;

		rx_buf[0] = (wx >> 8) & 0x00ff;
		rx_buf[1] = wx & 0x00ff;

		rx_buf[2] = (wy >> 8) & 0x00ff;
		rx_buf[3] = wy & 0x00ff;

		pr_info("%s: got wp info from cache\n", __func__);
		goto done;
	}

	/* try to get wp info from cmdline */
	if (sscanf(oled_wp_cmdline, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
			&rx_buf[13], &rx_buf[14], &rx_buf[0], &rx_buf[1],
			&rx_buf[2], &rx_buf[3]) == 6) {

		if (rx_buf[13] == 1 && rx_buf[14] == 2 && rx_buf[0] == 3 &&
			rx_buf[1] == 4 && rx_buf[2] == 5 && rx_buf[3] == 6) {
			pr_err("No panel is Connected !");
			goto err;
		}

		lux = rx_buf[13] << 8 | rx_buf[14];
		wx = rx_buf[0] << 8 | rx_buf[1];
		wy = rx_buf[2] << 8 | rx_buf[3];
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
	lux = rx_buf[13] << 8 | rx_buf[14];
	wx = rx_buf[0] << 8 | rx_buf[1];
	wy = rx_buf[2] << 8 | rx_buf[3];
done:
	count = snprintf(buf, size, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
			rx_buf[13], rx_buf[14], rx_buf[0], rx_buf[1],
			rx_buf[2], rx_buf[3]);

	pr_info("%s: Lux=0x%02hx, Wx=0x%02hx, Wy=0x%02hx\n", __func__, lux, wx, wy);
err:
	pr_info("%s: -\n", __func__);
	return count;
}

static int panel_set_spr_status(struct drm_panel *panel, int status)
{
	struct lcm *ctx;
	int ret = 0;
	char spr_status_cmd0[] = {0xF0, 0x5A, 0x5A};
	char spr_status_cmd1[] = {0xB0, 0x01, 0x1D, 0x92};
	char spr_status_cmd2[] = {0x92, status == SPR_2D_RENDERING ? 0x70 : 0x71};
	char spr_status_cmd3[] = {0xF0, 0xA5, 0xA5};
	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 4,
	};

	pr_info("%s: status = %d\n", __func__, status);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}

	ctx = panel_to_lcm(panel);
	if (ctx->spr_status == status) {
		pr_info("%s: status not changed, nothing to do\n", __func__);
		goto err;
	}
	ctx->spr_status = status;
	if (!ctx->enabled) {
		pr_info("%s: panel isn't enabled yet, spr status will be applied later\n", __func__);
		goto err;
	}

	cmd_msg.type[0] = ARRAY_SIZE(spr_status_cmd0) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = spr_status_cmd0;
	cmd_msg.tx_len[0] = ARRAY_SIZE(spr_status_cmd0);

	cmd_msg.type[1] = ARRAY_SIZE(spr_status_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = spr_status_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(spr_status_cmd1);

	cmd_msg.type[2] = ARRAY_SIZE(spr_status_cmd2) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[2] = spr_status_cmd2;
	cmd_msg.tx_len[2] = ARRAY_SIZE(spr_status_cmd2);

	cmd_msg.type[3] = ARRAY_SIZE(spr_status_cmd3) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[3] = spr_status_cmd3;
	cmd_msg.tx_len[3] = ARRAY_SIZE(spr_status_cmd3);

	ret = mtk_ddic_dsi_wait_te_send_cmd(&cmd_msg, true);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
err:
	return ret;

}
#endif

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);
	char display_off_tb[] = {0x28, 0x00};
	char backlight_off_tb[] = {0x51, 0x00, 0x00};
	char level2_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char normal2aod_tb[] = {0x53, 0x24}; // aod mode
	char update_key_tb[] ={0xF7, 0x0F};
	char level2_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	atomic_set(&doze_enable, 1);
	pr_info("%s !+\n", __func__);

	lcm_get_dcs_panel_id(ctx);

	if (!lcm_panel_is_p0())
		cb(dsi, handle, backlight_off_tb, ARRAY_SIZE(backlight_off_tb));
	else
		cb(dsi, handle, display_off_tb, ARRAY_SIZE(display_off_tb));
	cb(dsi, handle, level2_key_enable_tb, ARRAY_SIZE(level2_key_enable_tb));
	cb(dsi, handle, normal2aod_tb, ARRAY_SIZE(normal2aod_tb));
	cb(dsi, handle, update_key_tb, ARRAY_SIZE(update_key_tb));
	cb(dsi, handle, level2_key_disable_tb, ARRAY_SIZE(level2_key_disable_tb));
	usleep_range(40 * 1000, 40 * 1000 + 10);

	pr_info("%s !-\n", __func__);
	return 0;
}


static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = panel_to_lcm(panel);
	char level2_key_enable_tb[] = {0xF0, 0x5A, 0x5A};
	char aod2normal_tb[] = {0x53, 0x28}; // aod to normal mode
	char backlightoff[] = {0x51, 0x00, 0x00};
	char update_key_tb[] ={0xF7, 0x0F};
	char level2_key_disable_tb[] = {0xF0, 0xA5, 0xA5};
	char display_on_tb[] = {0x29, 0x00};

	pr_info("%s +\n", __func__);
	cb(dsi, handle, level2_key_enable_tb, ARRAY_SIZE(level2_key_enable_tb));
	cb(dsi, handle, aod2normal_tb, ARRAY_SIZE(aod2normal_tb));
	cb(dsi, handle, backlightoff, ARRAY_SIZE(backlightoff));
	cb(dsi, handle, update_key_tb, ARRAY_SIZE(update_key_tb));
	cb(dsi, handle, level2_key_disable_tb, ARRAY_SIZE(level2_key_disable_tb));
	cb(dsi, handle, display_on_tb, ARRAY_SIZE(display_on_tb));
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
	.mode_switch = mode_switch,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
#ifdef CONFIG_MI_DISP
	.init = lcm_panel_init,
	.get_panel_info = panel_get_panel_info,
	.get_panel_initialized = get_lcm_initialized,
	.esd_restore_backlight = lcm_esd_restore_backlight,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.panel_set_dc = panel_set_dc,
	.panel_get_dc = panel_get_dc,
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
	.get_panel_dynamic_fps =  panel_get_dynamic_fps,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
	.panel_elvss_control = panel_elvss_control,
	.get_wp_info = panel_get_wp_info,
	.set_spr_status = panel_set_spr_status,
	.panel_set_dc_crc = panel_set_dc_crc,
	.panel_set_dc_crc_bl_pack = panel_set_dc_crc_bl_pack,
	.panel_restore_crc_level = panel_restore_crc_level,
	.panel_set_dc_crc_off = panel_set_dc_crc_off,
	.set_dc_backlight = panel_set_dc_backlight,
	.set_dc_threshold = panel_set_dc_threshold,
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
	struct drm_display_mode *mode0, *mode1, *mode2, *mode3;

	mode0 = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode0) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode0);
	mode0->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode0);

	mode1 = drm_mode_duplicate(connector->dev, &performence_mode);
	if (!mode1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performence_mode.hdisplay, performence_mode.vdisplay,
			drm_mode_vrefresh(&performence_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode1);

	mode2 = drm_mode_duplicate(connector->dev, &default_mode_fhdp);
	if (!mode2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode_fhdp.hdisplay, default_mode_fhdp.vdisplay,
			drm_mode_vrefresh(&default_mode_fhdp));
		return -ENOMEM;
	}
	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

	mode3 = drm_mode_duplicate(connector->dev, &performence_mode_fhdp);
	if (!mode3) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performence_mode_fhdp.hdisplay, performence_mode_fhdp.vdisplay,
			drm_mode_vrefresh(&performence_mode_fhdp));
		return -ENOMEM;
	}
	drm_mode_set_name(mode3);
	mode3->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode3);

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
		pr_err("%s init vrf18_aif regulator error\n", __func__);

	ext_params.err_flag_irq_gpio = of_get_named_gpio_flags(
			dev->of_node, "mi,esd-err-irq-gpio",
			0, (enum of_gpio_flags *)&(ext_params.err_flag_irq_flags));

	ext_params_120hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_fhdp.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_fhdp.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_120hz_fhdp.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz_fhdp.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ctx->prepared = true;
	ctx->enabled = true;
	ctx->panel_info = panel_name;
	ctx->dynamic_fps = 60;
	ctx->wqhd_en = true;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	ctx->spr_status = SPR_2D_RENDERING;
	ctx->dc_status = false;
	ctx->crc_level = 0;

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

#ifdef CONFIG_MI_DISP_PANEL_COUNT
	dsi_panel_count_init(ctx);
#endif

	pr_info("l11a_38_0a_0a_dsc_cmd %s-\n", __func__);

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
	{ .compatible = "l11a_38_0a_0a_dsc_cmd,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "l11a_38_0a_0a_dsc_cmd,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

module_param_string(oled_wp, oled_wp_cmdline, sizeof(oled_wp_cmdline), 0600);
MODULE_PARM_DESC(oled_wp, "oled_wp=<white_point_info>");

MODULE_AUTHOR("Honghui Tang <tanghonghui@xiaomi.com>");
MODULE_DESCRIPTION("L11a 38 0a 0a dsc wqhd oled panel driver");
MODULE_LICENSE("GPL v2");
