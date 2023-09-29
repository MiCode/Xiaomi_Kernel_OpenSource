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

#include <uapi/drm/mi_disp.h>
#include "mi_panel_ext.h"
#include "mi_dsi_panel.h"
#include "include/panel-m12-42-02-0a-dsc-cmd.h"

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "mtk_panel_ext.h"
#include "mtk_drm_graphics_base.h"
#include "mtk_log.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#include "include/mi_disp_nvt_alpha_data.h"


#include <linux/atomic.h>

#if defined(CONFIG_PXLW_IRIS)
#include "dsi_iris_api.h"
#include "dsi_iris_mtk_api.h"
#endif

#define MAX_BRIGHTNESS_CLONE 	16383
#define FACTORY_MAX_BRIGHTNESS 	8191

#define PANEL_NONE_APL2600   0
#define PANEL_APL2600        1
#define PANEL_APL2600_MP     2

static unsigned char panel_build_id = PANEL_NONE_APL2600;
static char build_id_cmdline[4] = {0};

static char hbm_status = -1;
static char dbi_setting_status = 0;

static char bl_tb0[] = {0x51, 0x3, 0xff};
static int current_fps = 120;

static char oled_wp_cmdline[16] = {0};
static char oled_lhbm_cmdline[80] = {0};
//static char oled_grayscale_cmdline[100] = {0};

static bool lhbm_w900_update_flag = true;
static bool lhbm_w110_update_flag = true;
static bool lhbm_g500_update_flag = true;
static bool lhbm_w900_readbackdone;
static bool lhbm_w110_readbackdone;
static bool lhbm_g500_readbackdone;
struct LHBM_WHITEBUF {
	unsigned char gir_off_110[6];
	unsigned char gir_on_110[6];
	unsigned char gir_off_900[6];
        unsigned char gir_on_900[6];
	unsigned char gir_off_500[2];
	unsigned char gir_on_500[2];
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

enum bl_level_DBI_setting_range {
	DBI_SETTING_OFF = 0,
	DBI_SETTING_1,
	DBI_SETTING_2,
	DBI_SETTING_3,
	DBI_SETTING_4,
};

static atomic_t doze_enable = ATOMIC_INIT(0);
static bool reloadgamma_readbackdone;
static struct lcm *panel_ctx;

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
#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris() && iris_is_pt_mode()) {
		ret = 0;
		iris_panel_cmd_passthrough((int)*addr, len -1, (u8 *)&data[1], NULL, 0, 0);
		pr_err("IRIS:%s %p name =%s\n", __func__, dsi, dsi->name);
	} else {
		if ((int)*addr < 0xB0)
			ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
		else
			ret = mipi_dsi_generic_write(dsi, data, len);
	}
#else
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
#endif
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

//AMOLED 1.2V:DVDD_1P2,GPIO39
//AMOLED 1.8V:VDDIO_1P8,VAUD18
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

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct lcm *ctx;

	if (panel == NULL)
		return 0;
	ctx = panel_to_lcm(panel);

	if (ctx->prepared)
		return 0;

	pr_info("%s\n",__func__);
#if defined(CONFIG_RT5081_PMU_DSV) || defined(CONFIG_MT6370_PMU_DSV)
	lcm_panel_bias_disable();
#else
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
#endif
	udelay(1000);

#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris() && iris_is_chip_supported()) {
		iris_reset_off(ctx->dev);
	}
#endif

	lcm_panel_vci_disable(ctx->dev);
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
	udelay(1000);
	lcm_panel_vddi_disable(ctx->dev);

#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris() && iris_is_chip_supported()) {
		iris_power_off(ctx->dev);
	}
#endif
	return 0;
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
	pr_info("%s +");

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(125);

#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris() && iris_is_chip_supported()) {
		iris_disable(false, NULL);
		iris_set_valid(2);
	}
#endif

	ctx->error = 0;
	ctx->prepared = false;
	ctx->crc_level = 0;
	ctx->doze_suspend = false;
	ctx->need_auto_update_gamma = false;
	ctx->lhbm_en = false;

	atomic_set(&doze_enable, 0);
	pr_info("%s -");
	return 0;
}

static void lcm_get_build_id(void) {
	static bool is_update = false;

	if (is_update)
		return;

	pr_info("%s: build_id_cmdline:%s  +\n", __func__, build_id_cmdline);
	sscanf(build_id_cmdline, "%02hhx\n", &panel_build_id);
	is_update = true;
	return;
}

static void lcm_panel_init(struct lcm *ctx)
{
	pr_info("%s: +\n", __func__);
	if (ctx->prepared) {
		pr_info("%s: panel has been prepared, nothing to do!\n", __func__);
		goto err;
	}

#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris() && iris_is_chip_supported()) {
		iris_reset(ctx->dev);
	}
#endif
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		goto err;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(12 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(1 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(12 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris() && iris_is_chip_supported()) {
		iris_set_valid(3);
		iris_enable(NULL);
	}
#endif
	lcm_get_build_id();
	push_table(ctx, init_setting,
		sizeof(init_setting) / sizeof(struct LCM_setting_table));

	if (!reloadgamma_readbackdone)
			panel_parse_reloadgamma_info(&sReloadgamma_table);

	if (!panel_build_id) {
		if (gir_on_reload[1].cmd == 0xB4)
			gir_on_reload[1].para_list[0] = 0x13;
		else
			pr_err("%s: error need update\n", __func__);
	}

	if (ctx->gir_status) {
		push_table(ctx, gir_on_reload,
			sizeof(gir_on_reload) / sizeof(struct LCM_setting_table));

		if (ctx->dynamic_fps == 144) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_144hz_setting_gir_on_peak2600,
					sizeof(mode_144hz_setting_gir_on_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_144hz_setting_gir_on,
					sizeof(mode_144hz_setting_gir_on) / sizeof(struct LCM_setting_table));
			}
		} else if (ctx->dynamic_fps == 120) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_120hz_setting_gir_on_peak2600,
					sizeof(mode_120hz_setting_gir_on_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_120hz_setting_gir_on,
					sizeof(mode_120hz_setting_gir_on) / sizeof(struct LCM_setting_table));
			}
		} else if (ctx->dynamic_fps == 90) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_90hz_setting_gir_on_peak2600,
					sizeof(mode_90hz_setting_gir_on_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_90hz_setting_gir_on,
					sizeof(mode_90hz_setting_gir_on) / sizeof(struct LCM_setting_table));
			}
		} else if (ctx->dynamic_fps == 60) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_60hz_setting_gir_on_peak2600,
					sizeof(mode_60hz_setting_gir_on_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_60hz_setting_gir_on,
					sizeof(mode_60hz_setting_gir_on) / sizeof(struct LCM_setting_table));
			}
		}

		push_table(ctx, gir_on_gamma_update,
			sizeof(gir_on_gamma_update) / sizeof(struct LCM_setting_table));

	} else {
		if (ctx->dynamic_fps == 144) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_144hz_setting_gir_off_peak2600,
					sizeof(mode_144hz_setting_gir_off_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_144hz_setting_gir_off,
					sizeof(mode_144hz_setting_gir_off) / sizeof(struct LCM_setting_table));
			}
		} else if (ctx->dynamic_fps == 120) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_120hz_setting_gir_off_peak2600,
					sizeof(mode_120hz_setting_gir_off_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_120hz_setting_gir_off,
					sizeof(mode_120hz_setting_gir_off) / sizeof(struct LCM_setting_table));
			}
		} else if (ctx->dynamic_fps == 90) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_90hz_setting_gir_off_peak2600,
					sizeof(mode_90hz_setting_gir_off_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_90hz_setting_gir_off,
					sizeof(mode_90hz_setting_gir_off) / sizeof(struct LCM_setting_table));
			}
		} else if (ctx->dynamic_fps == 60) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_60hz_setting_gir_off_peak2600,
					sizeof(mode_60hz_setting_gir_off_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_60hz_setting_gir_off,
					sizeof(mode_60hz_setting_gir_off) / sizeof(struct LCM_setting_table));
			}
		}
		push_table(ctx, gir_off_gamma_update,
			sizeof(gir_off_gamma_update) / sizeof(struct LCM_setting_table));
	}

	push_table(ctx, sleep_out_setting,
		sizeof(sleep_out_setting) / sizeof(struct LCM_setting_table));
	if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
		if (ctx->gray_level == TEMP_INDEX1_35) {
			gray_3d_lut_on[3].para_list[0] = 0x02;
			push_table(ctx, gray_3d_lut_on,
				sizeof(gray_3d_lut_on) / sizeof(struct LCM_setting_table));
		} else if (ctx->gray_level == TEMP_INDEX2_30) {
			gray_3d_lut_on[3].para_list[0] = 0x01;
			push_table(ctx, gray_3d_lut_on,
				sizeof(gray_3d_lut_on) / sizeof(struct LCM_setting_table));
		} else if (ctx->gray_level == TEMP_INDEX3_28) {
			gray_3d_lut_on[3].para_list[0] = 0x00;
			push_table(ctx, gray_3d_lut_on,
				sizeof(gray_3d_lut_on) / sizeof(struct LCM_setting_table));
		} else if (ctx->gray_level == TEMP_INDEX4_off ) {
			 push_table(ctx, gray_3d_lut_off,
				sizeof(gray_3d_lut_off) / sizeof(struct LCM_setting_table));
		}
	}
	ctx->prepared = true;
	ctx->doze_suspend = false;
	ctx->peak_hdr_status = 0;

#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris() && iris_is_chip_supported()) {
		iris_set_valid(4);
	}
#endif

err:
	pr_info("%s: -\n", __func__);
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
	ctx->need_auto_update_gamma = false;

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

	if (ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = true;

	return 0;
}

static int lcm_panel_poweron(struct drm_panel *panel)
{
	struct lcm *ctx;

	if (panel == NULL)
		return 0;
	ctx = panel_to_lcm(panel);

	if (ctx->prepared)
		return 0;

#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris() && iris_is_chip_supported()) {
		iris_power_on(ctx->dev);
	}
#endif

	pr_info("%s\n",__func__);
	ctx->cam_gpio = devm_gpiod_get_index(ctx->dev,
		"cam", 0, GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->cam_gpio)) {
		dev_err(ctx->dev, "%s: cannot get cam gpio %ld\n",
			__func__, PTR_ERR(ctx->cam_gpio));
	}
	gpiod_set_value(ctx->cam_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->cam_gpio);

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
	udelay(1 * 1000);

	return 0;
}

static const struct drm_display_mode default_mode = {
	.clock = 273139,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE0_HFP,
	.hsync_end = FRAME_WIDTH + MODE0_HFP + MODE0_HSA,
	.htotal = FRAME_WIDTH + MODE0_HFP + MODE0_HSA + MODE0_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE0_VFP + MODE0_VSA,
	.vtotal = FRAME_HEIGHT + MODE0_VFP + MODE0_VSA + MODE0_VBP,
};

static const struct drm_display_mode middle_mode = {
	.clock = 347526,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE2_HFP,
	.hsync_end = FRAME_WIDTH + MODE2_HFP + MODE2_HSA,
	.htotal = FRAME_WIDTH + MODE2_HFP + MODE2_HSA + MODE2_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE2_VFP + MODE2_VSA,
	.vtotal = FRAME_HEIGHT + MODE2_VFP + MODE2_VSA + MODE2_VBP,
};

static const struct drm_display_mode performence_mode = {
	.clock = 463368,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE1_HFP,
	.hsync_end = FRAME_WIDTH + MODE1_HFP + MODE1_HSA,
	.htotal = FRAME_WIDTH + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE1_VFP + MODE1_VSA,
	.vtotal = FRAME_HEIGHT + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

static const struct drm_display_mode performence_mode_144 = {
	.clock = 556041,
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE3_HFP,
	.hsync_end = FRAME_WIDTH + MODE3_HFP + MODE3_HSA,
	.htotal = FRAME_WIDTH + MODE3_HFP + MODE3_HSA + MODE3_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE3_VFP,
	.vsync_end = FRAME_HEIGHT + MODE3_VFP + MODE3_VSA,
	.vtotal = FRAME_HEIGHT + MODE3_VFP + MODE3_VSA + MODE3_VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = DATA_RATE1 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_od = true,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
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
		.ext_pps_cfg = {
			.enable = 1,
			.range_min_qp = m12_42_dphy_range_min_qp,
			.range_max_qp = m12_42_dphy_range_max_qp,
			.range_bpg_ofs = m12_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE1,
	//.lp_perline_en = 1,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,
#endif
};

static struct mtk_panel_params ext_params_90hz = {
	.pll_clk = DATA_RATE1 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_od = true,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
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
		.ext_pps_cfg = {
			.enable = 1,
			.range_min_qp = m12_42_dphy_range_min_qp,
			.range_max_qp = m12_42_dphy_range_max_qp,
			.range_bpg_ofs = m12_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE1,
	//.lp_perline_en = 1,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,

#endif
};

static struct mtk_panel_params ext_params_120hz = {
	.pll_clk = DATA_RATE1 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_od = true,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
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
		.ext_pps_cfg = {
			.enable = 1,
			.range_min_qp = m12_42_dphy_range_min_qp,
			.range_max_qp = m12_42_dphy_range_max_qp,
			.range_bpg_ofs = m12_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE1,
	//.lp_perline_en = 0,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,

#endif
};

static struct mtk_panel_params ext_params_144hz = {
	.pll_clk = DATA_RATE3 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.is_support_od = true,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
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
		.ext_pps_cfg = {
			.enable = 1,
			.range_min_qp = m12_42_dphy_range_min_qp,
			.range_max_qp = m12_42_dphy_range_max_qp,
			.range_bpg_ofs = m12_42_dphy_range_bpg_ofs,
			},
		},
	.data_rate = DATA_RATE3,
	//.lp_perline_en = 1,
#ifdef CONFIG_MI_DISP_FOD_SYNC
	.bl_sync_enable = 1,
	.aod_delay_enable = 1,
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
	.round_corner_en = 1,
	.corner_pattern_height = ROUND_CORNER_H_TOP,
	.corner_pattern_height_bot = ROUND_CORNER_H_BOT,
	.corner_pattern_tp_size_l = sizeof(top_rc_pattern_l),
	.corner_pattern_lt_addr_l = (void *)top_rc_pattern_l,
	.corner_pattern_tp_size_r = sizeof(top_rc_pattern_r),
	.corner_pattern_lt_addr_r = (void *)top_rc_pattern_r,

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
	int dst_fps = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;

	if (dst_fps == MODE0_FPS)
		*ext_param = &ext_params;
	else if (dst_fps == MODE2_FPS)
		*ext_param = &ext_params_90hz;
	else if (dst_fps == MODE1_FPS)
		*ext_param = &ext_params_120hz;
	else if (dst_fps == MODE3_FPS)
		*ext_param = &ext_params_144hz;
	else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

	if (!ret)
		current_fps = dst_fps;

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	int dst_fps = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;

	if (dst_fps == MODE0_FPS)
		ext->params = &ext_params;
	else if (dst_fps == MODE2_FPS)
		ext->params = &ext_params_90hz;
	else if (dst_fps == MODE1_FPS)
		ext->params = &ext_params_120hz;
	else if (dst_fps == MODE3_FPS)
		ext->params = &ext_params_144hz;
	else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}

	if (!ret)
		current_fps = dst_fps;

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

#if defined(CONFIG_PXLW_IRIS)
static void iris_setbacklight(void *dsi, dcs_write_gce cb,
		void *handle, char *src, int s_len)
{
	char dest[512] = {0,};
	int len = 0;
	len = iris_conver_one_panel_cmd(dest, src, s_len);
	if (len > 0)
		cb(dsi, handle, dest, len);
}
#endif

static void lcm_set_hbm_demura(void *dsi, dcs_write_gce cb,
	void *handle, bool enable)
{
	char cmd0_hbm_demura[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x04};
	char cmd1_hbm_demura[] = {0x6F, 0x10};
	char cmd2_hbm_demura[] = {0xCB, 0x80, 0x20};
	char cmd3_hbm_demura[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	char cmd4_hbm_demura[] = {0x6F, 0x1C};
	char cmd5_hbm_demura[] = {0xC0, 0x22, 0x44, 0x22, 0x00, 0x55};
	char cmd0_lbm_demura[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x04};
	char cmd1_lbm_demura[] = {0x6F, 0x10};
	char cmd2_lbm_demura[] = {0xCB, 0x80, 0x80};
	char cmd3_lbm_demura[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	char cmd4_lbm_demura[] = {0x6F, 0x1C};
	char cmd5_lbm_demura[] = {0xC0, 0x00, 0x33, 0x00, 0x00, 0x11};

	pr_info("%s: +, enable: %d.\n", __func__, enable);
	if (enable == true) {
		cb(dsi, handle, cmd0_hbm_demura, ARRAY_SIZE(cmd0_hbm_demura));
		cb(dsi, handle, cmd1_hbm_demura, ARRAY_SIZE(cmd1_hbm_demura));
		cb(dsi, handle, cmd2_hbm_demura, ARRAY_SIZE(cmd2_hbm_demura));
		cb(dsi, handle, cmd3_hbm_demura, ARRAY_SIZE(cmd3_hbm_demura));
		cb(dsi, handle, cmd4_hbm_demura, ARRAY_SIZE(cmd4_hbm_demura));
		cb(dsi, handle, cmd5_hbm_demura, ARRAY_SIZE(cmd5_hbm_demura));
	} else {
		cb(dsi, handle, cmd0_lbm_demura, ARRAY_SIZE(cmd0_lbm_demura));
		cb(dsi, handle, cmd1_lbm_demura, ARRAY_SIZE(cmd1_lbm_demura));
		cb(dsi, handle, cmd2_lbm_demura, ARRAY_SIZE(cmd2_lbm_demura));
		cb(dsi, handle, cmd3_lbm_demura, ARRAY_SIZE(cmd3_lbm_demura));
		cb(dsi, handle, cmd4_lbm_demura, ARRAY_SIZE(cmd4_lbm_demura));
		cb(dsi, handle, cmd5_lbm_demura, ARRAY_SIZE(cmd5_lbm_demura));
	}

	pr_info("%s: -\n", __func__);
	return;
}

static int lcm_handle_hbm_dbi_setting(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	static int last_hbm_dbi_status = -1;

	/* for dbv <= 0x7ff(2047) or dbv >= 0xE11(3601)*/
	if (level <= DBI_SETTING_1_THRESHOLD || level > DBI_SETTING_OFF_THRESHOLD) {
		dbi_setting_status = DBI_SETTING_OFF;
	/* for dbv >= 0x800(2048) and dbv <= 0x915(2325)*/
	} else if (level > DBI_SETTING_1_THRESHOLD && level <= DBI_SETTING_2_THRESHOLD) {
		dbi_setting_status = DBI_SETTING_1;
	/* for dbv >= 0x916(2326) and dbv <= 0xA8C(2700)*/
	} else if (level > DBI_SETTING_2_THRESHOLD && level <= DBI_SETTING_3_THRESHOLD) {
		dbi_setting_status = DBI_SETTING_2;
	/* for dbv >= 0xA8D(2701) and dbv <= 0xC4E(3150)*/
	} else if (level > DBI_SETTING_3_THRESHOLD && level <= DBI_SETTING_4_THRESHOLD) {
		dbi_setting_status = DBI_SETTING_3;
	/* for dbv >= 0xC4F(3151) and dbv <= 0xE10(3600)*/
	} else if (level > DBI_SETTING_4_THRESHOLD && level <= DBI_SETTING_OFF_THRESHOLD) {
		dbi_setting_status = DBI_SETTING_4;
	}

	if (dbi_setting_status != last_hbm_dbi_status) {
		pr_info("%s enter update, DBI setting = %d", __func__, dbi_setting_status);
		switch (dbi_setting_status) {
		case DBI_SETTING_OFF:
			cb(dsi, handle, CMD_PAGE8, ARRAY_SIZE(CMD_PAGE8));
			cb(dsi, handle, DBI_SETTING_OFF_R, ARRAY_SIZE(DBI_SETTING_OFF_R));
			cb(dsi, handle, DBI_SETTING_OFF_G, ARRAY_SIZE(DBI_SETTING_OFF_G));
			cb(dsi, handle, DBI_SETTING_OFF_B, ARRAY_SIZE(DBI_SETTING_OFF_B));
			break;
		case DBI_SETTING_1:
			cb(dsi, handle, CMD_PAGE8, ARRAY_SIZE(CMD_PAGE8));
			cb(dsi, handle, DBI_SETTING_1_R, ARRAY_SIZE(DBI_SETTING_1_R));
			cb(dsi, handle, DBI_SETTING_1_G, ARRAY_SIZE(DBI_SETTING_1_G));
			cb(dsi, handle, DBI_SETTING_1_B, ARRAY_SIZE(DBI_SETTING_1_B));
			break;
		case DBI_SETTING_2:
			cb(dsi, handle, CMD_PAGE8, ARRAY_SIZE(CMD_PAGE8));
			cb(dsi, handle, DBI_SETTING_2_R, ARRAY_SIZE(DBI_SETTING_2_R));
			cb(dsi, handle, DBI_SETTING_2_G, ARRAY_SIZE(DBI_SETTING_2_G));
			cb(dsi, handle, DBI_SETTING_2_B, ARRAY_SIZE(DBI_SETTING_2_B));
			break;
		case DBI_SETTING_3:
			cb(dsi, handle, CMD_PAGE8, ARRAY_SIZE(CMD_PAGE8));
			cb(dsi, handle, DBI_SETTING_3_R, ARRAY_SIZE(DBI_SETTING_3_R));
			cb(dsi, handle, DBI_SETTING_3_G, ARRAY_SIZE(DBI_SETTING_3_G));
			cb(dsi, handle, DBI_SETTING_3_B, ARRAY_SIZE(DBI_SETTING_3_B));
			break;
		case DBI_SETTING_4:
			cb(dsi, handle, CMD_PAGE8, ARRAY_SIZE(CMD_PAGE8));
			cb(dsi, handle, DBI_SETTING_4_R, ARRAY_SIZE(DBI_SETTING_4_R));
			cb(dsi, handle, DBI_SETTING_4_G, ARRAY_SIZE(DBI_SETTING_4_G));
			cb(dsi, handle, DBI_SETTING_4_B, ARRAY_SIZE(DBI_SETTING_4_B));
			break;
		default:
			pr_err("%s not support dbi status!", __func__);
			break;
		}
		last_hbm_dbi_status = dbi_setting_status;
	}

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	struct lcm *ctx = NULL;
	char bl_tb[] = {0x51, 0x07, 0xff};
	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;
	if (!mtk_dsi || !mtk_dsi->panel) {
		pr_err("dsi is null\n");
		return -1;
	}
	ctx = panel_to_lcm(mtk_dsi->panel);

	lcm_get_build_id();
	if (panel_build_id == PANEL_APL2600_MP) {
		if (level >= 2048 && hbm_status != 1) {
			lcm_set_hbm_demura(dsi, cb, handle, true);
			hbm_status = 1;
		} else if (level < 2048 && hbm_status != 0) {
			lcm_set_hbm_demura(dsi, cb, handle, false);
			hbm_status = 0;
		}
	}

	/* for peak2600 DBI setting */
	if (panel_build_id == PANEL_APL2600_MP || panel_build_id == PANEL_APL2600) {
		lcm_handle_hbm_dbi_setting(dsi, cb, handle, level);
	}

#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris() && iris_is_pt_mode()) {
		if (level) {
			int min_level = 8;
			level = (level > min_level) ? level : min_level;
		}
	}
#endif

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
		pr_info("%s: Return it when aod on, %d %d %d\n",
			__func__, level, bl_tb[1], bl_tb[2]);
		return 0;
	}

	//panel_set_gamma_update_reg(mtk_dsi->panel,level);
	mtk_dsi->mi_cfg.last_bl_level = level;
	pr_info("%s %d %d %d\n", __func__, level, bl_tb[1], bl_tb[2]);
#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris())
		iris_setbacklight(dsi, cb, handle, bl_tb, ARRAY_SIZE(bl_tb));
	else
		cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
#else
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
#endif

	return 0;
}

static int lcm_set_bl_elvss_cmdq(void *dsi, dcs_grp_write_gce cb, void *handle,
		struct mtk_bl_ext_config *bl_ext_config)
{
	int pulses;
	int level;
	bool need_cfg_elvss = false;

	static struct mtk_panel_para_table bl_tb[] = {
			{3, {0x51, 0x0f, 0xff}},
		};
	static struct mtk_panel_para_table bl_elvss_tb[] = {
			{3, { 0x51, 0x0f, 0xff}},
			{2, { 0x83, 0xff}},
		};
	static struct mtk_panel_para_table elvss_tb[] = {
			{2, { 0x83, 0xff}},
		};

	if (!cb)
		return -1;

	pulses = bl_ext_config->elvss_pn;
	level = bl_ext_config->backlight_level;

	if (bl_ext_config->cfg_flag & (0x1<<SET_BACKLIGHT_LEVEL)) {

		if (bl_ext_config->cfg_flag & (0x1<<SET_ELVSS_PN))
			need_cfg_elvss = true;

		if (atomic_read(&doze_enable)) {
			pr_info("%s: Return it when aod on, %d %d %d\n",
				__func__, level, bl_tb[0].para_list[1], bl_tb[0].para_list[2]);
			if (need_cfg_elvss) {
				pr_info("%s elvss = -%d\n", __func__, pulses);
				elvss_tb[0].para_list[1] = (u8)((1<<7)|pulses);
//#if defined(CONFIG_PXLW_IRIS)
#if 0
				iris_setbacklight(dsi, cb, handle, elvss_tb, ARRAY_SIZE(elvss_tb));
#else
				cb(dsi, handle, elvss_tb, ARRAY_SIZE(elvss_tb));
#endif
			}
			return 0;
		}

		if (need_cfg_elvss) {
			bl_elvss_tb[0].para_list[1] = (level >> 8) & 0xf;
			bl_elvss_tb[0].para_list[2] = (level) & 0xFF;
			pr_info("%s backlight = -%d\n", __func__, level);
			pr_info("%s elvss = -%d\n", __func__, pulses);
			bl_elvss_tb[1].para_list[1] = (u8)((1<<7)|pulses);
			cb(dsi, handle, bl_elvss_tb, ARRAY_SIZE(bl_elvss_tb));
		} else {
			pr_info("%s backlight = -%d\n", __func__, level);
			bl_tb[0].para_list[1] = (level >> 8) & 0xf;
			bl_tb[0].para_list[2] = (level) & 0xFF;
			cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
		}

	} else if (bl_ext_config->cfg_flag & (0x1<<SET_ELVSS_PN)) {
		pr_info("%s elvss = -%d\n", __func__, pulses);
		elvss_tb[0].para_list[1] = (u8)((1<<7)|pulses);
		cb(dsi, handle, elvss_tb, ARRAY_SIZE(elvss_tb));
	}
	return 0;
}

static void panel_update_gir_setting()
{
	int i = 0;
	pr_info("%s +\n", __func__);
	for (i = 0; i < 4; i++) {
		// for mode switch
		if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
			mode_144hz_setting_gir_off_peak2600[sGiroff_144hz_Index].para_list[i] = sReloadgamma_table.gir_off_144[i];
			mode_144hz_setting_gir_on_peak2600[sGiron_144hz_Index].para_list[i] = sReloadgamma_table.gir_on_144[i];

			mode_120hz_setting_gir_off_peak2600[sGiroff_120hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			mode_120hz_setting_gir_on_peak2600[sGiron_120hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];

			mode_90hz_setting_gir_off_peak2600[sGiroff_90hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			mode_90hz_setting_gir_on_peak2600[sGiron_90hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];

			mode_60hz_setting_gir_off_peak2600[sGiroff_60hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			mode_60hz_setting_gir_on_peak2600[sGiron_60hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];

			pr_info("mode_switch peak gamma setting: 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx\n",
					mode_144hz_setting_gir_off_peak2600[sGiroff_144hz_Index].para_list[i],
					mode_144hz_setting_gir_on_peak2600[sGiron_144hz_Index].para_list[i],
					mode_120hz_setting_gir_off_peak2600[sGiroff_120hz_Index].para_list[i],
					mode_120hz_setting_gir_on_peak2600[sGiron_120hz_Index].para_list[i],
					mode_90hz_setting_gir_off_peak2600[sGiroff_90hz_Index].para_list[i],
					mode_90hz_setting_gir_on_peak2600[sGiron_90hz_Index].para_list[i],
					mode_60hz_setting_gir_off_peak2600[sGiroff_60hz_Index].para_list[i],
					mode_60hz_setting_gir_on_peak2600[sGiron_60hz_Index].para_list[i]);

			// for gir switch
			gir_off_120hz_settings[sGir_Switch_off_120hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];
			gir_off_120hz_settings[sGir_Switch_off_120hz_Index+1].para_list[i] = sReloadgamma_table.gir_off_120[i];
			gir_on_120hz_settings[sGir_Switch_on_120hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];
			gir_on_120hz_settings[sGir_Switch_on_120hz_Index+1].para_list[i] = sReloadgamma_table.gir_off_120[i];

			gir_off_not_120hz_settings_peak2600[sGir_Switch_off_not_60hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			gir_on_not_120hz_settings_peak2600[sGir_Switch_on_not_60hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];

			gir_off_144hz_peak2600[sGiroff_144hz_Index].para_list[i] = sReloadgamma_table.gir_off_144[i];
			gir_on_144hz_peak2600[sGiron_144hz_Index].para_list[i] = sReloadgamma_table.gir_on_144[i];

			gir_off_90hz_peak2600[sGiroff_90hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			gir_on_90hz_peak2600[sGiron_90hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];

			gir_off_60hz_peak2600[sGiroff_60hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			gir_on_60hz_peak2600[sGiron_60hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];
		} else {
			mode_144hz_setting_gir_off[sGiroff_144hz_Index].para_list[i] = sReloadgamma_table.gir_off_144[i];
			mode_144hz_setting_gir_on[sGiron_144hz_Index].para_list[i] = sReloadgamma_table.gir_on_144[i];

			mode_120hz_setting_gir_off[sGiroff_120hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			mode_120hz_setting_gir_on[sGiron_120hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];

			mode_90hz_setting_gir_off[sGiroff_90hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			mode_90hz_setting_gir_on[sGiron_90hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];

			mode_60hz_setting_gir_off[sGiroff_60hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			mode_60hz_setting_gir_on[sGiron_60hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];

			pr_info("mode_switch gamma setting: 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx\n",
					mode_144hz_setting_gir_off[sGiroff_144hz_Index].para_list[i],
					mode_144hz_setting_gir_on[sGiron_144hz_Index].para_list[i],
					mode_120hz_setting_gir_off[sGiroff_120hz_Index].para_list[i],
					mode_120hz_setting_gir_on[sGiron_120hz_Index].para_list[i],
					mode_90hz_setting_gir_off[sGiroff_90hz_Index].para_list[i],
					mode_90hz_setting_gir_on[sGiron_90hz_Index].para_list[i],
					mode_60hz_setting_gir_off[sGiroff_60hz_Index].para_list[i],
					mode_60hz_setting_gir_on[sGiron_60hz_Index].para_list[i]);
			
			// for gir switch
			gir_off_120hz_settings[sGir_Switch_off_120hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];
			gir_off_120hz_settings[sGir_Switch_off_120hz_Index+1].para_list[i] = sReloadgamma_table.gir_off_120[i];
			gir_on_120hz_settings[sGir_Switch_on_120hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];
			gir_on_120hz_settings[sGir_Switch_on_120hz_Index+1].para_list[i] = sReloadgamma_table.gir_off_120[i];

			gir_off_not_120hz_settings[sGir_Switch_off_not_60hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			gir_on_not_120hz_settings[sGir_Switch_on_not_60hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];

			gir_off_144hz[sGiroff_144hz_Index].para_list[i] = sReloadgamma_table.gir_off_144[i];
			gir_on_144hz[sGiron_144hz_Index].para_list[i] = sReloadgamma_table.gir_on_144[i];

			gir_off_90hz[sGiroff_90hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			gir_on_90hz[sGiron_90hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];

			gir_off_60hz[sGiroff_60hz_Index].para_list[i] = sReloadgamma_table.gir_off_120[i];
			gir_on_60hz[sGiron_60hz_Index].para_list[i] = sReloadgamma_table.gir_on_120[i];
		}
	}
	pr_info("%s -\n", __func__);
}

static void panel_parse_reloadgamma_info(struct reloadgamma_table *reloadgamma_table)
{
	int i;
	static u8 reload_gamma_info[16] = {0};

	pr_info("%s: +\n", __func__);

	pr_info("%s: reload_gamma_info_cmdline:%s\n", __func__, reload_gamma_info_cmdline);
	for (i = 0; i < 16; i++) {
		sscanf(reload_gamma_info_cmdline + 2 * i, "%02hhx", &reload_gamma_info[i]);
	}

	for (i = 0; i < 4; i++) {
		reloadgamma_table->gir_on_120[i] = reload_gamma_info[i];
		reloadgamma_table->gir_off_120[i] = reload_gamma_info[i + 4];
		reloadgamma_table->gir_on_144[i] = reload_gamma_info[i + 8];
		reloadgamma_table->gir_off_144[i] = reload_gamma_info[i + 12];
	}

	pr_info("gir_on_120:\n");
	for (i = 0; i < 4; i++) {
		pr_info("0x%02hhx",reloadgamma_table->gir_on_120[i]);
	}

	pr_info("gir_off_120:\n");
	for (i = 0; i < 4; i++) {
		pr_info("0x%02hhx",reloadgamma_table->gir_off_120[i]);
	}

	pr_info("gir_on_144:\n");
	for (i = 0; i < 4; i++) {
		pr_info("0x%02hhx",reloadgamma_table->gir_on_144[i]);
	}

	pr_info("gir_off_144:\n");
	for (i = 0; i < 4; i++) {
		pr_info("0x%02hhx",reloadgamma_table->gir_off_144[i]);
	}

	reloadgamma_readbackdone = true;

	panel_update_gir_setting();
	pr_info("%s: -\n", __func__);
}

static void mode_switch_to_144(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);

	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_144hz_setting_gir_on_peak2600,
					sizeof(mode_144hz_setting_gir_on_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_144hz_setting_gir_on,
					sizeof(mode_144hz_setting_gir_on) / sizeof(struct LCM_setting_table));
			}
		} else {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_144hz_setting_gir_off_peak2600,
					sizeof(mode_144hz_setting_gir_off_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_144hz_setting_gir_off,
					sizeof(mode_144hz_setting_gir_off) / sizeof(struct LCM_setting_table));
			}
		}
		ctx->dynamic_fps = 144;
	}
}

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);

	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_120hz_setting_gir_on_peak2600,
					sizeof(mode_120hz_setting_gir_on_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_120hz_setting_gir_on,
					sizeof(mode_120hz_setting_gir_on) / sizeof(struct LCM_setting_table));
			}
		} else {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_120hz_setting_gir_off_peak2600,
					sizeof(mode_120hz_setting_gir_off_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_120hz_setting_gir_off,
					sizeof(mode_120hz_setting_gir_off) / sizeof(struct LCM_setting_table));
			}
		}

		ctx->dynamic_fps = 120;
	}
}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);

	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_90hz_setting_gir_on_peak2600,
					sizeof(mode_90hz_setting_gir_on_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_90hz_setting_gir_on,
					sizeof(mode_90hz_setting_gir_on) / sizeof(struct LCM_setting_table));
			}
		} else {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_90hz_setting_gir_off_peak2600,
					sizeof(mode_90hz_setting_gir_off_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_90hz_setting_gir_off,
					sizeof(mode_90hz_setting_gir_off) / sizeof(struct LCM_setting_table));
			}
		}
		ctx->dynamic_fps = 90;
	}
}

static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);

	if (ctx->lhbm_en) {
		push_table(ctx, lhbm_off_without51,
			sizeof(lhbm_off_without51) / sizeof(struct LCM_setting_table));
		ctx->lhbm_en = false;
		pr_info("%s need LHBM OFF!\n", __func__);
	}

	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status) {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_60hz_setting_gir_on_peak2600,
					sizeof(mode_60hz_setting_gir_on_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_60hz_setting_gir_on,
					sizeof(mode_60hz_setting_gir_on) / sizeof(struct LCM_setting_table));
			}
		} else {
			if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
				push_table(ctx, mode_60hz_setting_gir_off_peak2600,
					sizeof(mode_60hz_setting_gir_off_peak2600) / sizeof(struct LCM_setting_table));
			} else {
				push_table(ctx, mode_60hz_setting_gir_off,
					sizeof(mode_60hz_setting_gir_off) / sizeof(struct LCM_setting_table));
			}
		}
		ctx->dynamic_fps = 60;
	}
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	int dst_fps = 0, cur_fps = 0;
	int dst_vdisplay = 0, dst_hdisplay = 0;
	int cur_vdisplay = 0, cur_hdisplay = 0;
	bool isFpsChange = false;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);
	struct lcm *ctx = panel_to_lcm(panel);

	if (cur_mode == dst_mode)
		return ret;

	lcm_get_build_id();
	if (!reloadgamma_readbackdone)
		panel_parse_reloadgamma_info(&sReloadgamma_table);

	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;
	dst_vdisplay = m_dst ? m_dst->vdisplay : -EINVAL;
	dst_hdisplay = m_dst ? m_dst->hdisplay : -EINVAL;
	cur_fps = m_cur ? drm_mode_vrefresh(m_cur) : -EINVAL;
	cur_vdisplay = m_cur ? m_cur->vdisplay : -EINVAL;
	cur_hdisplay = m_cur ? m_cur->hdisplay : -EINVAL;

	isFpsChange = ((dst_fps == cur_fps) && (dst_fps != -EINVAL)
			&& (cur_fps != -EINVAL)) ? false : true;

	pr_info("%s isFpsChange = %d\n", __func__, isFpsChange);
	pr_info("%s dst_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, dst_fps, dst_vdisplay, dst_hdisplay);
	pr_info("%s cur_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, cur_fps, cur_vdisplay, cur_hdisplay);

	if (isFpsChange) {
		if (dst_fps == MODE0_FPS)
			mode_switch_to_60(panel, stage);
		else if (dst_fps == MODE2_FPS)
			mode_switch_to_90(panel, stage);
		else if (dst_fps == MODE1_FPS)
			mode_switch_to_120(panel, stage);
		else if (dst_fps == MODE3_FPS)
			mode_switch_to_144(panel, stage);
		else
			ret = 1;

		ctx->last_mode_switch_time = ktime_get();
		ctx->last_refresh_rate = cur_fps;
	}

	return ret;
}

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	atomic_set(&doze_enable, 1);
	pr_info("%s !-\n", __func__);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = NULL;
	char page0_tb[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	char data_remap_enable_tb[] = {0xB2, 0x98};
	char cmd0_tb[] = {0x65, 0x00};
	char cmd1_tb[] = {0x38, 0x00};
	char page_51[] = {0x51, 0x00, 0x00};
	char cmd2_tb[] = {0x2C, 0x00};

#ifdef CONFIG_FACTORY_BUILD
	pr_info("%s factory\n", __func__);
	return 0;
#endif

	if (!dsi) {
		pr_err("%s dsi is null\n", __func__);
		return -1;
	}

	if (!panel) {
		pr_err("%s invalid panel\n", __func__);
		return -1;
	}

	pr_info("%s +\n", __func__);
	ctx = panel_to_lcm(panel);
	if (!ctx) {
		pr_err("%s ctx is null\n", __func__);
		return -1;
	}

#if 0
	if (!ctx->doze_suspend) {
		pr_info("%s not in doze suspend status, change bl only\n", __func__);
#if defined(CONFIG_PXLW_IRIS)
		if (is_mi_dev_support_iris()) {
			iris_setbacklight(dsi, cb, handle, page_51, ARRAY_SIZE(page_51));
		} else {
			cb(dsi, handle, page_51, ARRAY_SIZE(page_51));
		}
#else
		cb(dsi, handle, page_51, ARRAY_SIZE(page_51));
#endif
		goto exit;
	}
#endif

#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris()) {
		iris_setbacklight(dsi, cb, handle, page0_tb, ARRAY_SIZE(page0_tb));
		iris_setbacklight(dsi, cb, handle, data_remap_enable_tb, ARRAY_SIZE(data_remap_enable_tb));
		iris_setbacklight(dsi, cb, handle, cmd0_tb, ARRAY_SIZE(cmd0_tb));
		iris_setbacklight(dsi, cb, handle, cmd1_tb, ARRAY_SIZE(cmd1_tb));
		iris_setbacklight(dsi, cb, handle, page_51, ARRAY_SIZE(page_51));
		//usleep_range(50 * 1000, 50 * 1000 + 10);
		iris_setbacklight(dsi, cb, handle, cmd2_tb, ARRAY_SIZE(cmd2_tb));
	} else {
		cb(dsi, handle, page0_tb, ARRAY_SIZE(page0_tb));
		cb(dsi, handle, data_remap_enable_tb, ARRAY_SIZE(data_remap_enable_tb));
		cb(dsi, handle, cmd0_tb, ARRAY_SIZE(cmd0_tb));
		cb(dsi, handle, cmd1_tb, ARRAY_SIZE(cmd1_tb));
		cb(dsi, handle, page_51, ARRAY_SIZE(page_51));
		//usleep_range(50 * 1000, 50 * 1000 + 10);
		cb(dsi, handle, cmd2_tb, ARRAY_SIZE(cmd2_tb));
	}
#else
	cb(dsi, handle, page0_tb, ARRAY_SIZE(page0_tb));
	cb(dsi, handle, data_remap_enable_tb, ARRAY_SIZE(data_remap_enable_tb));
	cb(dsi, handle, cmd0_tb, ARRAY_SIZE(cmd0_tb));
	cb(dsi, handle, cmd1_tb, ARRAY_SIZE(cmd1_tb));
	cb(dsi, handle, page_51, ARRAY_SIZE(page_51));
	//usleep_range(50 * 1000, 50 * 1000 + 10);
	cb(dsi, handle, cmd2_tb, ARRAY_SIZE(cmd2_tb));
#endif

//exit:
	ctx->doze_suspend = false;
	atomic_set(&doze_enable, 0);
	pr_info("%s -\n", __func__);

	return 0;
}

static int panel_doze_suspend (struct drm_panel *panel,void * dsi, dcs_write_gce cb, void *handle) {
	struct lcm *ctx = NULL;
	char gir_off_tb[] = {0x5F, 0x00};
	char normal2aod_tb[] = {0x51, 0x00, 0x14, 0x01,0X55};
	char page0_tb[] = {0xF0,0x55, 0xAA, 0x52, 0x08, 0x00};
	char data_remap_diable_tb[] ={0xB2, 0x18};
	char cmd0_tb[] = {0x39, 0x00};
	char cmd1_tb1[] = {0x65, 0x01};
	char cmd2_tb2[] = {0x2C, 0x00};

	if (!dsi) {
		pr_err("%s dsi is null\n", __func__);
		return -1;
	}

	if (!panel) {
		pr_err("%s invalid panel\n", __func__);
		return -1;
	}

#ifdef CONFIG_FACTORY_BUILD
	pr_info("%s factory\n", __func__);
	return 0;
#endif

	ctx = panel_to_lcm(panel);

	if (!ctx) {
		pr_err("ctx is null\n");
		return -1;
	}

	if (ctx->doze_suspend) {
		pr_info("%s already suspend, skip\n", __func__);
		goto exit;
	}


	if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM) {
		normal2aod_tb[2] = 0xF6;
		normal2aod_tb[3] = 0x0F;
		normal2aod_tb[4] = 0xFE;
	}


#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris()) {
		iris_setbacklight(dsi, cb, handle, gir_off_tb, ARRAY_SIZE(gir_off_tb));
		iris_setbacklight(dsi, cb, handle, normal2aod_tb, ARRAY_SIZE(normal2aod_tb));
		iris_setbacklight(dsi, cb, handle, page0_tb, ARRAY_SIZE(page0_tb));
		iris_setbacklight(dsi, cb, handle, data_remap_diable_tb, ARRAY_SIZE(data_remap_diable_tb));
		iris_setbacklight(dsi, cb, handle, cmd0_tb, ARRAY_SIZE(cmd0_tb));
		usleep_range(20 * 1000, 20 * 1000 + 10);
		iris_setbacklight(dsi, cb, handle, cmd1_tb1, ARRAY_SIZE(cmd1_tb1));
		iris_setbacklight(dsi, cb, handle, cmd2_tb2, ARRAY_SIZE(cmd2_tb2));
	} else {
		cb(dsi, handle, gir_off_tb, ARRAY_SIZE(gir_off_tb));
		cb(dsi, handle, normal2aod_tb, ARRAY_SIZE(normal2aod_tb));
		cb(dsi, handle, page0_tb, ARRAY_SIZE(page0_tb));
		cb(dsi, handle, data_remap_diable_tb, ARRAY_SIZE(data_remap_diable_tb));
		cb(dsi, handle, cmd0_tb, ARRAY_SIZE(cmd0_tb));
		usleep_range(20 * 1000, 20 * 1000 + 10);
		cb(dsi, handle, cmd1_tb1, ARRAY_SIZE(cmd1_tb1));
		cb(dsi, handle, cmd2_tb2, ARRAY_SIZE(cmd2_tb2));
	}
#else
	cb(dsi, handle, gir_off_tb, ARRAY_SIZE(gir_off_tb));
	cb(dsi, handle, normal2aod_tb, ARRAY_SIZE(normal2aod_tb));
	cb(dsi, handle, page0_tb, ARRAY_SIZE(page0_tb));
	cb(dsi, handle, data_remap_diable_tb, ARRAY_SIZE(data_remap_diable_tb));
	cb(dsi, handle, cmd0_tb, ARRAY_SIZE(cmd0_tb));
	usleep_range(20 * 1000, 20 * 1000 + 10);
	cb(dsi, handle, cmd1_tb1, ARRAY_SIZE(cmd1_tb1));
	cb(dsi, handle, cmd2_tb2, ARRAY_SIZE(cmd2_tb2));
#endif
	pr_info("csot lhbm enter aod from doze_suspend\n");

	ctx->doze_suspend = true;
exit:
		pr_info("%s !-\n", __func__);
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

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
	struct lcm *ctx;
	int ret = 0;
#ifndef CONFIG_FACTORY_BUILD
	struct LCM_setting_table backlight_h[] = {
			{0x51, 04, {0x00, 0xF6, 0x0F, 0xFE}},
	};
	struct LCM_setting_table backlight_l[] = {
			{0x51, 04, {0x00, 0x14, 0x01, 0x55}},
	};
	struct LCM_setting_table backlight_0[] = {
			{0x51, 04, {0x00, 0x00, 0x00, 0x00}},
	};
#endif

	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}

	ctx = panel_to_lcm(panel);

	if (ctx->doze_brightness_state == doze_brightness) {
		pr_info("%s skip same doze_brightness set:%d\n", __func__, doze_brightness);
		return 0;
	}

	if (!atomic_read(&doze_enable)) {
		pr_info("%s normal mode cannot set doze brightness\n", __func__);
		goto exit;
	}

#ifdef CONFIG_FACTORY_BUILD
	if (DOZE_TO_NORMAL == doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(doze_disable_t, ARRAY_SIZE(doze_disable_t), false, false);
		usleep_range(50 * 1000, 50 * 1000 + 10);
		if (ret == -1)
			pr_info("%s error\n", __func__);
		ret = mi_disp_panel_ddic_send_cmd(table_2C, ARRAY_SIZE(table_2C), false, false);
		atomic_set(&doze_enable, 0);
		ctx->doze_suspend = false;
		goto exit;
	} else if (DOZE_BRIGHTNESS_LBM	== doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(doze_enable_l, ARRAY_SIZE(doze_enable_l), false, false);

	} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(doze_enable_h, ARRAY_SIZE(doze_enable_h), false, false);
	}
	usleep_range(20 * 1000, 20 * 1000 + 10);
	ret = mi_disp_panel_ddic_send_cmd(table_652C, ARRAY_SIZE(table_652C), false, false);
#else
	if (DOZE_TO_NORMAL == doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(backlight_0, ARRAY_SIZE(backlight_0), false, false);
		atomic_set(&doze_enable, 0);
	} else if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(backlight_l, ARRAY_SIZE(backlight_l), false, false);

	} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
		ret = mi_disp_panel_ddic_send_cmd(backlight_h, ARRAY_SIZE(backlight_h), false, false);
	}

#endif

	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}

exit :
	ctx->doze_brightness_state = doze_brightness;
	pr_info("%s end -\n", __func__);
	return ret;
}

static int panel_set_gray_by_temperature (struct drm_panel *panel, int level)
{
	int ret = 0;
	struct lcm *ctx;
	pr_info("%s: +\n", __func__);
	lcm_get_build_id();
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	if (!panel_build_id) {
		pr_err("%s: panel is not p2.0\n", __func__);
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

	pr_info("%s: level = %x\n", __func__, level);
	ctx->gray_level = level;
	if (level == TEMP_INDEX1_35) {
		gray_3d_lut_on[3].para_list[0] = 0x02;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut_on, ARRAY_SIZE(gray_3d_lut_on), false, false);
	} else if (level == TEMP_INDEX2_30) {
		gray_3d_lut_on[3].para_list[0] = 0x01;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut_on, ARRAY_SIZE(gray_3d_lut_on), false, false);
	} else if (level == TEMP_INDEX3_28) {
		gray_3d_lut_on[3].para_list[0] = 0x00;
		mi_disp_panel_ddic_send_cmd(gray_3d_lut_on, ARRAY_SIZE(gray_3d_lut_on), false, false);
	} else if (level == TEMP_INDEX4_off ) {
		mi_disp_panel_ddic_send_cmd(gray_3d_lut_off, ARRAY_SIZE(gray_3d_lut_off), false, false);
	}

err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_set_gir_on(struct drm_panel *panel)
{
	int ret = 0;
	struct lcm *ctx;

	pr_info("%s: +\n", __func__);

	lcm_get_build_id();
	if (!reloadgamma_readbackdone)
		panel_parse_reloadgamma_info(&sReloadgamma_table);

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

	if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
		if (ctx->dynamic_fps == 120) {
			// demura reload + 60hz gir on setting
			mi_disp_panel_ddic_send_cmd(gir_on_120hz_settings, ARRAY_SIZE(gir_on_120hz_settings), true, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			// reload gir on + 120hz gir on setting
			mi_disp_panel_ddic_send_cmd(gir_on_120hz_reload, ARRAY_SIZE(gir_on_120hz_reload), false, false);
		} else if (ctx->dynamic_fps == 60) {
			// demura reload + 120hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_on_not_120hz_settings_peak2600, ARRAY_SIZE(gir_on_not_120hz_settings_peak2600), true, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			mi_disp_panel_ddic_send_cmd(fcon_reload_off2, ARRAY_SIZE(fcon_reload_off2), false, false);
			// 60hz gir on setting
			mi_disp_panel_ddic_send_cmd(gir_on_60hz_peak2600, ARRAY_SIZE(gir_on_60hz_peak2600), false, false);
		} else if (ctx->dynamic_fps == 90) {
			// demura reload + 120hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_on_not_120hz_settings_peak2600, ARRAY_SIZE(gir_on_not_120hz_settings_peak2600), true, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			mi_disp_panel_ddic_send_cmd(fcon_reload_off2, ARRAY_SIZE(fcon_reload_off2), false, false);
			// 90hz gir on setting
			mi_disp_panel_ddic_send_cmd(gir_on_90hz_peak2600, ARRAY_SIZE(gir_on_90hz_peak2600), false, false);
		} else if (ctx->dynamic_fps == 144) {
			// demura reload + 120hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_on_not_120hz_settings_peak2600, ARRAY_SIZE(gir_on_not_120hz_settings_peak2600), false, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			mi_disp_panel_ddic_send_cmd(fcon_reload_off2, ARRAY_SIZE(fcon_reload_off2), false, false);
			// 144hz gir on setting
			mi_disp_panel_ddic_send_cmd(gir_on_144hz_peak2600, ARRAY_SIZE(gir_on_144hz_peak2600), false, false);
		}
	} else {
		if (ctx->dynamic_fps == 120) {
			// demura reload + 60hz gir on setting
			mi_disp_panel_ddic_send_cmd(gir_on_120hz_settings, ARRAY_SIZE(gir_on_120hz_settings), false, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			// reload gir on + 120hz gir on setting
			mi_disp_panel_ddic_send_cmd(gir_on_120hz_reload, ARRAY_SIZE(gir_on_120hz_reload), false, false);
		} else if (ctx->dynamic_fps == 60) {
			// demura reload + 120hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_on_not_120hz_settings, ARRAY_SIZE(gir_on_not_120hz_settings), false, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			mi_disp_panel_ddic_send_cmd(fcon_reload_off2, ARRAY_SIZE(fcon_reload_off2), false, false);
			// 60hz gir on setting
			mi_disp_panel_ddic_send_cmd(gir_on_60hz, ARRAY_SIZE(gir_on_60hz), false, false);
		} else if (ctx->dynamic_fps == 90) {
			// demura reload + 120hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_on_not_120hz_settings, ARRAY_SIZE(gir_on_not_120hz_settings), false, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			mi_disp_panel_ddic_send_cmd(fcon_reload_off2, ARRAY_SIZE(fcon_reload_off2), false, false);
			// 90hz gir on setting
			mi_disp_panel_ddic_send_cmd(gir_on_90hz, ARRAY_SIZE(gir_on_90hz), false, false);
		} else if (ctx->dynamic_fps == 144) {
			// demura reload + 120hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_on_not_120hz_settings, ARRAY_SIZE(gir_on_not_120hz_settings), false, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			mi_disp_panel_ddic_send_cmd(fcon_reload_off2, ARRAY_SIZE(fcon_reload_off2), false, false);
			// 144hz gir on setting
			mi_disp_panel_ddic_send_cmd(gir_on_144hz, ARRAY_SIZE(gir_on_144hz), false, false);
		}
	}


err:
	pr_info("%s: -\n", __func__);
	return ret;
}

static int panel_set_gir_off(struct drm_panel *panel)
{
	struct lcm *ctx;
	int ret = -1;

	pr_info("%s: +\n", __func__);
	lcm_get_build_id();
	if (!reloadgamma_readbackdone)
		panel_parse_reloadgamma_info(&sReloadgamma_table);

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

	if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
		if (ctx->dynamic_fps == 120) {
			// demura reload + 60hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_off_120hz_settings, ARRAY_SIZE(gir_off_120hz_settings), true, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			// reload gir off + 120hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_off_120hz_reload, ARRAY_SIZE(gir_off_120hz_reload), false, false);
		} else if (ctx->dynamic_fps == 60) {
			// demura reload + 120hz setting
			mi_disp_panel_ddic_send_cmd(gir_off_not_120hz_settings_peak2600, ARRAY_SIZE(gir_off_not_120hz_settings_peak2600), true, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			// auto update gamma
			mi_disp_panel_ddic_send_cmd(fcon_reload_off, ARRAY_SIZE(fcon_reload_off), false, false);
			// 60hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_off_60hz_peak2600, ARRAY_SIZE(gir_off_60hz_peak2600), false, false);
		} else if (ctx->dynamic_fps == 90) {
			// demura reload + 120hz setting
			mi_disp_panel_ddic_send_cmd(gir_off_not_120hz_settings_peak2600, ARRAY_SIZE(gir_off_not_120hz_settings_peak2600), true, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			// auto update gamma
			mi_disp_panel_ddic_send_cmd(fcon_reload_off, ARRAY_SIZE(fcon_reload_off), false, false);
			// 90hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_off_90hz_peak2600, ARRAY_SIZE(gir_off_90hz_peak2600), false, false);
		} else if (ctx->dynamic_fps == 144) {
			// demura reload + 120hz setting
			mi_disp_panel_ddic_send_cmd(gir_off_not_120hz_settings_peak2600, ARRAY_SIZE(gir_off_not_120hz_settings_peak2600), false, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			// auto update gamma
			mi_disp_panel_ddic_send_cmd(fcon_reload_off, ARRAY_SIZE(fcon_reload_off), false, false);
			// 144hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_off_144hz_peak2600, ARRAY_SIZE(gir_off_144hz_peak2600), false, false);
		}
	} else {
		if (ctx->dynamic_fps == 120) {
			// demura reload + 60hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_off_120hz_settings, ARRAY_SIZE(gir_off_120hz_settings), false, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			// reload gir off + 120hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_off_120hz_reload, ARRAY_SIZE(gir_off_120hz_reload), false, false);
		} else if (ctx->dynamic_fps == 60) {
			// demura reload + 120hz setting
			mi_disp_panel_ddic_send_cmd(gir_off_not_120hz_settings, ARRAY_SIZE(gir_off_not_120hz_settings), false, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			// auto update gamma
			mi_disp_panel_ddic_send_cmd(fcon_reload_off, ARRAY_SIZE(fcon_reload_off), false, false);
			// 60hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_off_60hz, ARRAY_SIZE(gir_off_60hz), false, false);
		} else if (ctx->dynamic_fps == 90) {
			// demura reload + 120hz setting
			mi_disp_panel_ddic_send_cmd(gir_off_not_120hz_settings, ARRAY_SIZE(gir_off_not_120hz_settings), false, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			// auto update gamma
			mi_disp_panel_ddic_send_cmd(fcon_reload_off, ARRAY_SIZE(fcon_reload_off), false, false);
			// 90hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_off_90hz, ARRAY_SIZE(gir_off_90hz), false, false);
		} else if (ctx->dynamic_fps == 144) {
			// demura reload + 120hz setting
			mi_disp_panel_ddic_send_cmd(gir_off_not_120hz_settings, ARRAY_SIZE(gir_off_not_120hz_settings), false, false);
			usleep_range(20 * 1000, 20 * 1000 + 10);
			// auto update gamma
			mi_disp_panel_ddic_send_cmd(fcon_reload_off, ARRAY_SIZE(fcon_reload_off), false, false);
			// 144hz gir off setting
			mi_disp_panel_ddic_send_cmd(gir_off_144hz, ARRAY_SIZE(gir_off_144hz), false, false);
		}
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

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	ctx = panel_to_lcm(panel);
	*max_brightness_clone = ctx->factory_max_brightness;

	return 0;
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

static void lcm_esd_restore_backlight(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));

	pr_info("%s high_bl = 0x%x, low_bl = 0x%x \n", __func__, bl_tb0[1], bl_tb0[2]);

	return;
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

static void mi_parse_cmdline_perBL(struct LHBM_WHITEBUF * lhbm_whitebuf) {
	int i = 0;
	int temp = 0;
	int gamma_coffee_w900_gir_on[3] = {1015,1015,1015};
	static u8 lhbm_cmdbuf[28] = {0};

	pr_info("mi_parse_cmdline_perBL enter\n");

	if(!lhbm_w900_update_flag && !lhbm_w110_update_flag && !lhbm_g500_update_flag) {
		pr_info("don't need update white rgb config");
		return;
	}

	if (lhbm_whitebuf == NULL) {
		pr_err("lhbm_status == NULL\n");
		return;
	}

	lcm_get_build_id();

	for (i = 0; i < 28; i++) {
		sscanf(oled_lhbm_cmdline + 2 * i, "%02hhx", &lhbm_cmdbuf[i]);
	}

	for (i = 0; i < 6; i++){
		lhbm_whitebuf->gir_off_110[i] = lhbm_cmdbuf[i];
		lhbm_whitebuf->gir_on_110[i] = lhbm_cmdbuf[i+6];
		lhbm_whitebuf->gir_off_900[i] = lhbm_cmdbuf[i+12];
		//lhbm_whitebuf->gir_on_900[i] = lhbm_cmdbuf[i+18];
	}

	for (i = 0; i < 6; i+=2) {
		if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
			temp = ((int)(lhbm_cmdbuf[i+18] * 256 + lhbm_cmdbuf[i+19]) *
								gamma_coffee_w900_gir_on[i/2]) / 1000;
		} else {
			temp = (int)(lhbm_cmdbuf[i+18] * 256 + lhbm_cmdbuf[i+19]);
		}
		lhbm_whitebuf->gir_on_900[i] = (temp & 0xFF00) >> 8;
		lhbm_whitebuf->gir_on_900[i+1] = temp & 0xFF;
	}

	for (i = 0; i < 2; i++) {
		if (panel_build_id == PANEL_APL2600 || panel_build_id == PANEL_APL2600_MP) {
			lhbm_whitebuf->gir_off_500[i] = lhbm_cmdbuf[i+24];
			lhbm_whitebuf->gir_on_500[i] = lhbm_cmdbuf[i+26];
		} else {
			lhbm_whitebuf->gir_off_500[i] = lhbm_cmdbuf[i+24]*1035/1000;
			lhbm_whitebuf->gir_on_500[i] = lhbm_cmdbuf[i+26]*1035/1000;
		}
	}

	pr_info("gir_off_250 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx",lhbm_whitebuf->gir_off_110[i]);
	}

	pr_info("gir_on_250 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx ",lhbm_whitebuf->gir_on_110[i]);
	}

	pr_info("gir_off_1200 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx ",lhbm_whitebuf->gir_off_900[i]);
	}

	pr_info("gir_on_1200 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx ",lhbm_whitebuf->gir_on_900[i]);
	}
	pr_info("\n gir 500 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx,\n",
		lhbm_whitebuf->gir_off_500[0],lhbm_whitebuf->gir_off_500[1],
		lhbm_whitebuf->gir_on_500[0], lhbm_whitebuf->gir_on_500[1]);

	lhbm_w900_readbackdone = true;
	lhbm_w110_readbackdone = true;
	lhbm_g500_readbackdone = true;
	lhbm_w900_update_flag = false;
	lhbm_w110_update_flag = false;
	lhbm_g500_update_flag =false;

	return;
}

static int mi_disp_panel_update_lhbm_white_param(struct mtk_dsi * dsi, enum lhbm_cmd_type type, int flat_mode)
{
	int i = 0;

	if(!dsi)
		return -EINVAL;

	/*
	if (!panel->panel_initialized) {
		DISP_ERROR("[%s] Panel not initialized\n", panel->type);
		rc = -EINVAL;
		goto exit;
	}
	*/

	if(!lhbm_w900_readbackdone ||
		 !lhbm_w110_readbackdone ||
		 !lhbm_g500_readbackdone) {
		pr_info("mi_disp_panel_update_lhbm_white_param cmdline_lhbm:%s\n", oled_lhbm_cmdline);

		mi_parse_cmdline_perBL(&lhbm_whitebuf);
	}

	pr_info("lhbm update 0xD0, lhbm_cmd_type:%d flat_mode:%d fps?\n", type, flat_mode);

	switch (type) {
	case TYPE_WHITE_1200:
		if (flat_mode) {
			for (i = 0; i < 6; i++){
				lhbm_normal_white_1200nit[8].para_list[i] = lhbm_whitebuf.gir_on_900[i];
			}
		} else {
			for (i = 0; i < 6; i++){
				lhbm_normal_white_1200nit[8].para_list[i] = lhbm_whitebuf.gir_off_900[i];
			}
		}
		break;
	case TYPE_WHITE_250:
		if (flat_mode) {
			for (i = 0; i < 6; i++){
				lhbm_normal_white_250nit[8].para_list[i] = lhbm_whitebuf.gir_on_110[i];
			}
		} else {
			for (i = 0; i < 6; i++){
				lhbm_normal_white_250nit[8].para_list[i] = lhbm_whitebuf.gir_off_110[i];
			}
		}
		break;
	case TYPE_GREEN_500:
		if (flat_mode) {
			for (i = 0; i < 2; i++){
				lhbm_normal_green_500nit[8].para_list[i+2] = lhbm_whitebuf.gir_on_500[i];
			}
		} else {
			for (i = 0; i < 2; i++){
				lhbm_normal_green_500nit[8].para_list[i+2] = lhbm_whitebuf.gir_off_500[i];
			}
		}
		pr_info("green 500 update flat mode off 120\n");
		break;
	case TYPE_HLPM_W1200:
		if (flat_mode) {
			for (i = 0; i < 6; i++){
				lhbm_hlpm_white_1200nit[14].para_list[i] = lhbm_whitebuf.gir_on_900[i];
			}
		} else {
			for (i = 0; i < 6; i++){
				lhbm_hlpm_white_1200nit[14].para_list[i] = lhbm_whitebuf.gir_off_900[i];
			}
		}
		break;
	case TYPE_HLPM_W250:
		if (flat_mode) {
			for (i = 0; i < 6; i++){
				lhbm_hlpm_white_250nit[14].para_list[i] = lhbm_whitebuf.gir_on_110[i];
			}
		} else {
			for (i = 0; i < 6; i++){
				lhbm_hlpm_white_250nit[14].para_list[i] = lhbm_whitebuf.gir_off_110[i];
			}
		}
		break;
	default:
		pr_err("unsuppport cmd \n");
	return -EINVAL;
	}

	return 0;
}

static void mi_disp_panel_update_lhbm_51reg(struct mtk_dsi *dsi,enum lhbm_cmd_type type , int bl_level) {
	u8 alpha_buf[2] = {0};
	struct lcm * ctx = NULL;

	if (!dsi || type >= TYPE_MAX) {
		pr_err("invalid params\n");
		return;
	}

	ctx = panel_to_lcm(dsi->panel);

	if (!ctx) {
		pr_err("ctx is null\n");
		return;
	}

	/*
	if (!panel->mi_cfg.lhbm_alpha_ctrlaa) {
		DISP_DEBUG("local hbm can't use alpha control AA area brightness\n");
		return 0;
	}
	 */

	pr_info("mi_disp_panel_update_lhbm_51reg[%d] bl_lvl = %d,\n",
		type, bl_level);

	alpha_buf[0] = (bl_level >> 8) & 0x0f;
	alpha_buf[0] = alpha_buf[0] | 0xf0;
	alpha_buf[1] = bl_level & 0xff;

	switch (type) {
	case TYPE_WHITE_1200:
		if(lhbm_normal_white_1200nit[12].cmd != 0x51){
			pr_err("update w900 0x51 error\n");
			return;
		}
		lhbm_normal_white_1200nit[12].para_list[0] = alpha_buf[0];
		lhbm_normal_white_1200nit[12].para_list[1] = alpha_buf[1];
		break;
	case TYPE_WHITE_250:
		if(lhbm_normal_white_250nit[12].cmd != 0x51){
			pr_err("update w110 0x51 error\n");
			return;
		}
		lhbm_normal_white_250nit[12].para_list[0] = alpha_buf[0];
		lhbm_normal_white_250nit[12].para_list[1] = alpha_buf[1];
		break;
	case TYPE_GREEN_500:
		if(lhbm_normal_green_500nit[12].cmd != 0x51){
			pr_err("update g500 0x51 error\n");
			return;
		}
		lhbm_normal_green_500nit[12].para_list[0] = alpha_buf[0];
		lhbm_normal_green_500nit[12].para_list[1] = alpha_buf[1];
		break;
	case TYPE_LHBM_OFF:
		if(lhbm_off[0].cmd != 0x51){
			pr_err("update g500 0x51 error\n");
			return;
		}
		lhbm_off[0].para_list[0] = alpha_buf[0];
		lhbm_off[0].para_list[1] = alpha_buf[1];
	case TYPE_HLPM_W1200:
		if(lhbm_hlpm_white_1200nit[18].cmd != 0x51){
			pr_err("update hlpm w900 0x51 error\n");
			return;
		}
		lhbm_hlpm_white_1200nit[18].para_list[0] = alpha_buf[0];
		lhbm_hlpm_white_1200nit[18].para_list[1] = alpha_buf[1];
		if(ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
			lhbm_hlpm_white_1200nit[0].para_list[2] = 0xF6;
		else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
			lhbm_hlpm_white_1200nit[0].para_list[2] = 0x14;
		break;
	case TYPE_HLPM_W250:
		if(lhbm_hlpm_white_250nit[18].cmd != 0x51){
			pr_err("update w110 0x51 error\n");
			return;
		}
		lhbm_hlpm_white_250nit[18].para_list[0] = alpha_buf[0];
		lhbm_hlpm_white_250nit[18].para_list[1] = alpha_buf[1];
		if(ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
			lhbm_hlpm_white_250nit[0].para_list[1] = 0xF6;
		else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
			lhbm_hlpm_white_250nit[0].para_list[1] = 0x14;
		break;
	default:
		pr_err("unsupport 51 type\n");
		return;
	}

	pr_info("mi_disp_panel_update_lhbm_51reg\n");
	return;

}

static void mi_disp_panel_update_lhbm_87reg(struct mtk_dsi *dsi,enum lhbm_cmd_type type , int bl_level) {
	u8 alpha_buf[2] = {0};

	if (!dsi || type >= TYPE_MAX) {
		pr_err("invalid params\n");
		return;
	}

	/*
	if (!panel->mi_cfg.lhbm_alpha_ctrlaa) {
		DISP_DEBUG("local hbm can't use alpha control AA area brightness\n");
		return 0;
	}
	 */

	pr_info("[%d] bl_lvl = %d, alpha = %d\n",
			type, bl_level,  aa_alpha_set[bl_level]);

	alpha_buf[0] = (aa_alpha_set[bl_level] >> 8) & 0x0f;
	alpha_buf[0] |= 0x10;
	alpha_buf[1] = aa_alpha_set[bl_level] & 0xff;
	pr_info("[%d] alpha_buf[0] = %02hhx, alpha_buf[1] = %02hhx\n",
		type, alpha_buf[0],  alpha_buf[1]);

	switch (type) {
	case TYPE_WHITE_1200:
		if(lhbm_normal_white_1200nit[15].cmd != 0x87){
			pr_err("update w900 0x87 error\n");
			return;
		}
		lhbm_normal_white_1200nit[15].para_list[0] = alpha_buf[0];
		lhbm_normal_white_1200nit[15].para_list[1] = alpha_buf[1];
		break;
	case TYPE_WHITE_250:
		if(lhbm_normal_white_250nit[15].cmd != 0x87){
			pr_err("update w110 0x87 error\n");
			return;
		}
		lhbm_normal_white_250nit[15].para_list[0] = alpha_buf[0];
		lhbm_normal_white_250nit[15].para_list[1] = alpha_buf[1];
		break;

	case TYPE_GREEN_500:
		if(lhbm_normal_green_500nit[15].cmd != 0x87){
			pr_err("update g500 0x87 error\n");
			return;
		}
		lhbm_normal_green_500nit[15].para_list[0] = alpha_buf[0];
		lhbm_normal_green_500nit[15].para_list[1] = alpha_buf[1];
		break;
	case TYPE_HLPM_W1200:
		if(lhbm_hlpm_white_1200nit[21].cmd != 0x87){
			pr_err("update hlpm w900 0x87 error\n");
			return;
		}
		lhbm_hlpm_white_1200nit[21].para_list[0] = alpha_buf[0];
		lhbm_hlpm_white_1200nit[21].para_list[1] = alpha_buf[1];
		break;
	case TYPE_HLPM_W250:
		if(lhbm_hlpm_white_250nit[16].cmd != 0x87){
			pr_err("update w110 0x87 error\n");
			return;
		}
		lhbm_hlpm_white_250nit[21].para_list[0] = alpha_buf[0];
		lhbm_hlpm_white_250nit[21].para_list[1] = alpha_buf[1];
		break;
	default:
		pr_err("unsupport update 0x87 type\n");
		return;
	}

	pr_info("mi_disp_panel_update_lhbm_87reg end\n");
	return;
}

static int panel_set_lhbm_fod(struct mtk_dsi *dsi, enum local_hbm_state lhbm_state)
{
	struct lcm *ctx = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;
	int bl_level;
	int flat_mode;
	int bl_level_doze = 242;

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
			bl_level_doze = 20;
	else if (atomic_read(&doze_enable) &&
		ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
			bl_level_doze = 242;
	else
			bl_level_doze = mi_cfg->last_no_zero_bl_level;
	pr_info("%s local hbm_state:%d \n",__func__, lhbm_state);
	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL:
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
		pr_info("LOCAL_HBM_OFF\n");
		if (bl_level)
			mi_disp_panel_update_lhbm_51reg(dsi, TYPE_LHBM_OFF, bl_level);
		else
			mi_disp_panel_update_lhbm_51reg(dsi, TYPE_LHBM_OFF, bl_level_doze);
		mi_disp_panel_ddic_send_cmd(lhbm_off,ARRAY_SIZE(lhbm_off), false, true);
		ctx->lhbm_en = false;
		break;
	case LOCAL_HBM_NORMAL_WHITE_750NIT:
		break;
	case LOCAL_HBM_NORMAL_WHITE_500NIT:
		break;
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
		pr_info("LOCAL_HBM_NORMAL_WHITE_250NIT\n");
		if (atomic_read(&doze_enable)) {
			mi_disp_panel_update_lhbm_51reg(dsi, TYPE_HLPM_W250, bl_level_doze);
			mi_disp_panel_update_lhbm_87reg(dsi, TYPE_HLPM_W250, bl_level_doze);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W250, flat_mode);
			mi_disp_panel_ddic_send_cmd(lhbm_hlpm_white_250nit,ARRAY_SIZE(lhbm_hlpm_white_250nit), false, true);
			ctx->doze_suspend = false;
		} else {
			mi_disp_panel_update_lhbm_51reg(dsi, TYPE_WHITE_250, bl_level);
			mi_disp_panel_update_lhbm_87reg(dsi, TYPE_WHITE_250, bl_level);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_WHITE_250, flat_mode);
			mi_disp_panel_ddic_send_cmd(lhbm_normal_white_250nit,ARRAY_SIZE(lhbm_normal_white_250nit), false, true);
		}
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		pr_info("LOCAL_HBM_NORMAL_GREEN_500NIt\n");
		mi_disp_panel_update_lhbm_51reg(dsi, TYPE_GREEN_500, bl_level);
		mi_disp_panel_update_lhbm_87reg(dsi, TYPE_GREEN_500, bl_level);
		mi_disp_panel_update_lhbm_white_param(dsi, TYPE_GREEN_500, flat_mode);
		mi_disp_panel_ddic_send_cmd(lhbm_normal_green_500nit,ARRAY_SIZE(lhbm_normal_green_500nit), false, true);
		ctx->doze_suspend = false;
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		if (atomic_read(&doze_enable)) {
			pr_info("for aod \n");
			mi_disp_panel_update_lhbm_51reg(dsi, TYPE_HLPM_W1200, bl_level_doze);
			mi_disp_panel_update_lhbm_87reg(dsi, TYPE_HLPM_W1200, bl_level_doze);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W1200, flat_mode);
			mi_disp_panel_ddic_send_cmd(lhbm_hlpm_white_1200nit,ARRAY_SIZE(lhbm_hlpm_white_1200nit), false, true);
			ctx->doze_suspend = false;
		} else {
			pr_info("LOCAL_HBM_NORMAL_WHITE_1200NIT in HBM\n");
			mi_disp_panel_update_lhbm_51reg(dsi, TYPE_WHITE_1200, bl_level);
			mi_disp_panel_update_lhbm_87reg(dsi, TYPE_WHITE_1200, bl_level);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_WHITE_1200, flat_mode);
			mi_disp_panel_ddic_send_cmd(lhbm_normal_white_1200nit,ARRAY_SIZE(lhbm_normal_white_1200nit), false, true);
		}
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		pr_info("LOCAL_HBM_HLPM_WHITE_250NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_disp_panel_update_lhbm_87reg(dsi, TYPE_HLPM_W250, bl_level_doze);
		if (atomic_read(&doze_enable)) {
			mi_disp_panel_update_lhbm_51reg(dsi, TYPE_HLPM_W250, bl_level_doze);
		}
		mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W250, flat_mode);
		mi_disp_panel_ddic_send_cmd(lhbm_hlpm_white_250nit,ARRAY_SIZE(lhbm_hlpm_white_250nit), false, true);
		ctx->doze_suspend = false;
		ctx->lhbm_en = true;
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
		pr_info("LOCAL_HBM_HLPM_WHITE_1200NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_disp_panel_update_lhbm_87reg(dsi, TYPE_HLPM_W1200, bl_level_doze);
		if (atomic_read(&doze_enable)) {
			mi_disp_panel_update_lhbm_51reg(dsi, TYPE_HLPM_W1200, bl_level_doze);
		}
		mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W1200, flat_mode);
		mi_disp_panel_ddic_send_cmd(lhbm_hlpm_white_1200nit,ARRAY_SIZE(lhbm_hlpm_white_1200nit), false, true);
		ctx->doze_suspend = false;
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
static int panel_fod_state_check (void * dsi, dcs_write_gce cb, void *handle)
{
	struct lcm *ctx = NULL;
	struct mtk_dsi *mtk_dsi = (struct mtk_dsi *)dsi;

	if (!mtk_dsi || !mtk_dsi->panel) {
		pr_err("dsi is null\n");
		return -1;
	}
	ctx = panel_to_lcm(mtk_dsi->panel);

	if (ctx->lhbm_en) {
		char lhbm_off_page1[] = {0x87, 0x0F, 0xFF};
		char lhbm_off_page2[] = {0x88, 0x01};
		char lhbm_off_page3[] = {0x86, 0x01};
		cb(dsi, handle, lhbm_off_page1, ARRAY_SIZE(lhbm_off_page1));
		cb(dsi, handle, lhbm_off_page2, ARRAY_SIZE(lhbm_off_page2));
		cb(dsi, handle, lhbm_off_page3, ARRAY_SIZE(lhbm_off_page3));
		ctx->lhbm_en = false;
		pr_info("%s set lhbm off\n", __func__);
	} else {
		pr_info("%s lhbm not enable\n", __func__);
	}

	pr_info("%s !-\n", __func__);
	return 0;
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
	dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod = 5;
	dsi->mi_cfg.local_hbm_enabled = 1;

	return 0;
}

int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	/* Read 8 bytes from 0xA1 register
	 * 	BIT[0-1] = Lux
	 * 	BIT[2-4] = Wx
	 * 	BIT[5-7] = Wy */
	static uint16_t lux = 0, wx = 0, wy = 0;
	int count = 0;
	u8 rx_buf[8] = {0x00};
	pr_info("%s: +\n", __func__);

	/* try to get wp info from cache */
	if (lux > 0 && wx > 0 && wy > 0) {
		pr_info("%s: got wp info from cache\n", __func__);
		goto cache;
	}

	/* try to get wp info from cmdline */
	if (sscanf(oled_wp_cmdline, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
			&rx_buf[0], &rx_buf[1], &rx_buf[2], &rx_buf[3],
			&rx_buf[4], &rx_buf[5]) == 6) {

		if (rx_buf[0] == 1 && rx_buf[1] == 2 && rx_buf[2] == 3 &&
			rx_buf[3] == 4 && rx_buf[4] == 5 && rx_buf[5] == 6) {
			pr_err("No panel is Connected !");
			goto err;
		}

		lux = rx_buf[0] << 8 | rx_buf[1];
		wx = rx_buf[2] << 8 | rx_buf[3];
		wy = rx_buf[4] << 8 | rx_buf[5];
		if (lux > 0 && wx > 0 && wy > 0) {
			pr_info("%s: got wp info from cmdline\n", __func__);
			goto done;
		}
	} else {
		pr_info("%s: get error\n", __func__);
		goto err;
	}

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

static int panel_set_peak_hdr_status(struct mtk_dsi *dsi, int status)
{
	struct lcm *ctx = NULL;
	struct mi_dsi_panel_cfg *mi_cfg = NULL;

	if (!dsi || !dsi->panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}

#ifdef CONFIG_FACTORY_BUILD
	pr_info("%s factory don't need to support.\n", __func__);
	return 0;
#endif

	ctx = panel_to_lcm(dsi->panel);
	mi_cfg = &dsi->mi_cfg;

	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}

	if (panel_build_id == PANEL_NONE_APL2600) {
		pr_info("%s:panel_build_id is PANEL_NONE_APL2600,skip set peak hdr status\n", __func__);
		goto out;
	}

	if (ctx->peak_hdr_status == status) {
		pr_info("%s:PEAK HDR is the same, return = %d\n", __func__, status);
		goto out;
	}

	if (status) {
		panel_set_gir_off(dsi->panel);
		mi_disp_panel_ddic_send_cmd(peak_hdr_on, ARRAY_SIZE(peak_hdr_on), false, false);
	}
	else {
		mi_disp_panel_ddic_send_cmd(peak_hdr_off, ARRAY_SIZE(peak_hdr_off), false, false);
		panel_set_gir_on(dsi->panel);
	}

	ctx->peak_hdr_status = status;
	pr_info("%s: peak_hdr_status = %d ctx->gir_status = %d\n", __func__, status, ctx->gir_status);
out:
	return 0;
err:
	return 1;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.set_bl_elvss_cmdq = lcm_set_bl_elvss_cmdq,
	.mode_switch = mode_switch,
	.panel_poweron = lcm_panel_poweron,
	.panel_poweroff = lcm_panel_poweroff,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
#ifdef CONFIG_MI_DISP
	.esd_restore_backlight = lcm_esd_restore_backlight,
	.panel_get_gir_status = panel_get_gir_status,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
	.get_panel_factory_max_brightness = panel_get_factory_max_brightness,
	.get_panel_initialized = get_panel_initialized,
	.get_panel_info = panel_get_panel_info,
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.doze_suspend = panel_doze_suspend,
	.set_lhbm_fod = panel_set_lhbm_fod,
	.panel_fod_lhbm_init = panel_fod_lhbm_init,
	.get_wp_info = panel_get_wp_info,
	.set_peak_hdr_status = panel_set_peak_hdr_status,
	.set_gray_by_temperature = panel_set_gray_by_temperature,
	.fod_state_check = panel_fod_state_check,
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

	mode2 = drm_mode_duplicate(connector->dev, &middle_mode);
	if (!mode2) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			middle_mode.hdisplay, middle_mode.vdisplay,
			drm_mode_vrefresh(&middle_mode));
		return -ENOMEM;
	}
	drm_mode_set_name(mode2);
	mode2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode2);

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

	mode3 = drm_mode_duplicate(connector->dev, &performence_mode_144);
	if (!mode3) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performence_mode_144.hdisplay, performence_mode_144.vdisplay,
			drm_mode_vrefresh(&performence_mode_144));
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
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	int ret;

	pr_info("%s m12-42-02-0a-dsc +\n", __func__);

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
	dsi->mode_flags = MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_NO_EOT_PACKET
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
	ext_params_90hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_90hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_120hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_120hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ext_params_144hz.err_flag_irq_gpio = ext_params.err_flag_irq_gpio;
	ext_params_144hz.err_flag_irq_flags = ext_params.err_flag_irq_flags;
	ctx->prepared = true;
	ctx->enabled = true;
	ctx->dynamic_fps = 60;
	ctx->gir_status = 1;
	ctx->peak_hdr_status = 0;
	ctx->panel_info = panel_name;
	ctx->panel_id = panel_id;
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
	ctx->factory_max_brightness = FACTORY_MAX_BRIGHTNESS;

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	#if defined(CONFIG_PXLW_IRIS)
	if (is_mi_dev_support_iris()) {
		ext_params.cust_esd_check = 1;
		ext_params.esd_check_enable = 1;
		ext_params_90hz.cust_esd_check = 1;
		ext_params_90hz.esd_check_enable = 1;
		ext_params_120hz.cust_esd_check = 1;
		ext_params_120hz.esd_check_enable = 1;
		ext_params_144hz.cust_esd_check = 1;
		ext_params_144hz.esd_check_enable = 1;
	}
	#endif
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif

	pr_info("%s m12-42-02-0a-dsc +\n", __func__);
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
	{ .compatible = "m12_42_02_0a_dsc_cmd,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "m12_42_02_0a_dsc_cmd,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

module_param_string(oled_lhbm, oled_lhbm_cmdline, sizeof(oled_lhbm_cmdline), 0600);
MODULE_PARM_DESC(oled_lhbm, "oled_lhbm=<local_hbm_info>");

module_param_string(oled_wp, oled_wp_cmdline, sizeof(oled_wp_cmdline), 0600);
MODULE_PARM_DESC(oled_wp, "oled_wp=<white_point_info>");

module_param_string(reload_gamma_info, reload_gamma_info_cmdline, sizeof(reload_gamma_info_cmdline), 0600);
MODULE_PARM_DESC(reload_gamma_info, "reload_gamma_info=<reload_gamma_table_info>");

module_param_string(build_id, build_id_cmdline, sizeof(build_id_cmdline), 0600);
MODULE_PARM_DESC(build_id, "build_id=<buildid_info>");

MODULE_AUTHOR("Sai Tian <tiansai1@xiaomi.com>");
MODULE_DESCRIPTION("m12_42_02_0a_dsc_cmd oled panel driver");
MODULE_LICENSE("GPL v2");
