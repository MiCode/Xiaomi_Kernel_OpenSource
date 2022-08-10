// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_panel.h>
#include <drm/drm_modes.h>
#include <linux/delay.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <linux/of_graph.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#include "panel-samsung-op-cmd.h"

#define HFP (40)
#define HSA (10)
#define HBP (20)
#define VFP (8)
#define VSA (8)
#define VBP (8)

#define VAC_WQHD (3216)
#define HAC_WQHD (1440)
#define VAC_FHD (2412)
#define HAC_FHD (1080)


enum h_skew_type {
	SDC_ADFR = 0,			/* SA */
	SDC_MFR = 1,			/* SM */
};

struct lcm_pmic_info {
	struct regulator *reg_vufs18;
	struct regulator *reg_vmch3p0;
};

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;
	struct gpio_desc *bias_gpio;
	struct gpio_desc *vddr1p5_enable_gpio;
	struct gpio_desc *te_switch_gpio, *te_out_gpio;
	struct drm_display_mode *m;
	bool prepared;
	bool enabled;
	int error;
};

static char bl_tb0[] = { 0x51, 0x07, 0xff};

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

static void lcm_dcs_write_ext(struct lcm *ctx, const void *data, size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;

	if (ctx->error < 0)
		return;

	ret = mipi_dsi_dcs_write_buffer(dsi, data, len);

	if (ret < 0) {
		pr_info("error %zd writing seq: %ph\n", ret, data);
		ctx->error = ret;
	}
}

static int get_mode_enum(struct drm_display_mode *m)
{
	int ret = 0, m_vrefresh = 0;

	if (m == NULL) {
		DDPMSG("%s display mode is null, return mode 0\n", __func__);
		return 0;
	}

	m_vrefresh = drm_mode_vrefresh(m);
	if (m->hdisplay == HAC_WQHD && m->vdisplay == VAC_WQHD) {
		if (m_vrefresh == 60 && m->hskew == SDC_MFR)
			ret = WQHD_SDC60;
		else if (m_vrefresh == 120 && m->hskew == SDC_MFR)
			ret = WQHD_SDC120;
		else if (m_vrefresh == 90 && m->hskew == SDC_MFR)
			ret = WQHD_SDC90;
		else
			DDPMSG("Invalid display mode\n");
	} else if (m->hdisplay == HAC_FHD && m->vdisplay == VAC_FHD) {
		if (m_vrefresh == 60 && m->hskew == SDC_MFR)
			ret = FHD_SDC60;
		else if (m_vrefresh == 120 && m->hskew == SDC_MFR)
			ret = FHD_SDC120;
		else if (m_vrefresh == 90 && m->hskew == SDC_MFR)
			ret = FHD_SDC90;
		else
			DDPMSG("Invalid display mode\n");
	}

	return ret;
}

static void push_table(struct lcm *ctx, struct LCM_setting_table *table,
	unsigned int count)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(table[i].count);
			break;
		case REGFLAG_UDELAY:
			usleep_range(table[i].count, table[i].count + 1000);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			lcm_dcs_write_ext(ctx,
				table[i].para_list,
				table[i].count);
			break;
		}
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
		dev_info(ctx->dev, "error %d reading dcs seq:(%#x)\n", ret, cmd);
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

static struct lcm_pmic_info *g_pmic;
static unsigned int lcm_get_reg_vufs18(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vufs18))
		volt = regulator_get_voltage(g_pmic->reg_vufs18);

	return volt;
}

static unsigned int lcm_get_reg_vmch3p0(void)
{
	unsigned int volt = 0;

	if (regulator_is_enabled(g_pmic->reg_vmch3p0))
		volt = regulator_get_voltage(g_pmic->reg_vmch3p0);

	return volt;
}

static unsigned int lcm_enable_reg_vufs18(int en)
{
	unsigned int ret = 0, volt = 0;

	if (en) {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vufs18)) {
			ret = regulator_enable(g_pmic->reg_vufs18);
			pr_info("[lh]Enable the Regulator vufs1p8ret=%d.\n", ret);
			volt = lcm_get_reg_vufs18();
			pr_info("[lh]get the Regulator vufs1p8 =%d.\n", volt);
		}
	} else {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vufs18)) {
			ret = regulator_disable(g_pmic->reg_vufs18);
			pr_info("[lh]disable the Regulator vufs1p8 ret=%d.\n", ret);
		}

	}
	return ret;

}

static unsigned int lcm_enable_reg_vmch3p0(int en)
{
	unsigned int ret = 0, volt = 0;

	if (en) {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vmch3p0)) {
			ret = regulator_enable(g_pmic->reg_vmch3p0);
			pr_info("[lh]Enable the Regulator vmch3p0 ret=%d.\n", ret);
			volt = lcm_get_reg_vmch3p0();
			pr_info("[lh]get the Regulator vmch3p0 =%d.\n", volt);
		}
	} else {
		if (!IS_ERR_OR_NULL(g_pmic->reg_vmch3p0)) {
			ret = regulator_disable(g_pmic->reg_vmch3p0);
			pr_info("[lh]disable the Regulator vmch3p0 ret=%d.\n", ret);
		}
	}
	return ret;

}

static void lcm_panel_init(struct lcm *ctx)
{
	int mode_id = -1;
	unsigned int count = 0;
	struct drm_display_mode *m = ctx->m;

	mode_id = get_mode_enum(m);
	DDPMSG("%s mode_id = %d\n", __func__, mode_id);

	switch (mode_id) {
	case WQHD_SDC60:
		count = sizeof(wqhd_dsi_on_cmd_sdc60)
			/ sizeof(struct LCM_setting_table);
		push_table(ctx, wqhd_dsi_on_cmd_sdc60, count);
		break;
	case WQHD_SDC120:
		count = sizeof(wqhd_dsi_on_cmd_sdc120)
			/ sizeof(struct LCM_setting_table);
		push_table(ctx, wqhd_dsi_on_cmd_sdc120, count);
		break;
	case WQHD_SDC90:
		count = sizeof(wqhd_dsi_on_cmd_sdc90)
			/ sizeof(struct LCM_setting_table);
		push_table(ctx, wqhd_dsi_on_cmd_sdc90, count);
		break;
	case FHD_SDC60:
		count = sizeof(fhd_dsi_on_cmd_sdc60)
			/ sizeof(struct LCM_setting_table);
		push_table(ctx, fhd_dsi_on_cmd_sdc60, count);
		break;
	case FHD_SDC120:
		count = sizeof(fhd_dsi_on_cmd_sdc120)
			/ sizeof(struct LCM_setting_table);
		push_table(ctx, fhd_dsi_on_cmd_sdc120, count);
		break;
	case FHD_SDC90:
		count = sizeof(fhd_dsi_on_cmd_sdc90)
			/ sizeof(struct LCM_setting_table);
		push_table(ctx, fhd_dsi_on_cmd_sdc90, count);
		break;
	default:
		DDPMSG("%s: default mode_id\n", __func__);
		count = sizeof(wqhd_dsi_on_cmd_sdc120)
			/ sizeof(struct LCM_setting_table);
		push_table(ctx, wqhd_dsi_on_cmd_sdc120, count);
		break;
	}

	//send 1024 for 0x51 on backlight resume flow
	lcm_dcs_write_seq_static(ctx, 0x51, 0x03, 0xff);
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

	lcm_dcs_write_seq_static(ctx, MIPI_DCS_ENTER_SLEEP_MODE);
	lcm_dcs_write_seq_static(ctx, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(200);

	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	ctx->vddr1p5_enable_gpio = devm_gpiod_get(ctx->dev, "vddr1p5-enable", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddr1p5_enable_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->vddr1p5_enable_gpio);
	msleep(20);
	lcm_enable_reg_vufs18(0);
	msleep(20);
	lcm_enable_reg_vmch3p0(0);

	ctx->error = 0;
	ctx->prepared = false;
	pr_info("[lh]%s-\n", __func__);

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("[lh]%s +\n", __func__);
	if (ctx->prepared)
		return 0;
	ret = lcm_enable_reg_vufs18(1);

	ctx->vddr1p5_enable_gpio = devm_gpiod_get(ctx->dev, "vddr1p5-enable", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->vddr1p5_enable_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->vddr1p5_enable_gpio);

	msleep(20);

	ret = lcm_enable_reg_vmch3p0(1);

	// lcd reset H -> L -> H
	msleep(20);
	ctx->reset_gpio = devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	gpiod_set_value(ctx->reset_gpio, 1);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value(ctx->reset_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	msleep(20);
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

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

static const struct drm_display_mode display_mode[MODE_NUM] = {
	//wqhd_sdc_60_mode
	{
		.clock = 292321,
		.hdisplay = HAC_WQHD,
		.hsync_start = HAC_WQHD + HFP,
		.hsync_end = HAC_WQHD + HFP + HSA,
		.htotal = HAC_WQHD + HFP + HSA + HBP,
		.vdisplay = VAC_WQHD,
		.vsync_start = VAC_WQHD + VFP,
		.vsync_end = VAC_WQHD + VFP + VSA,
		.vtotal = VAC_WQHD + VFP + VSA + VBP,
		.hskew = SDC_MFR,
	},
	//wqhd_sdc_120_mode
	{
		.clock = 584646,
		.hdisplay = HAC_WQHD,
		.hsync_start = HAC_WQHD + HFP,
		.hsync_end = HAC_WQHD + HFP + HSA,
		.htotal = HAC_WQHD + HFP + HSA + HBP,
		.vdisplay = VAC_WQHD,
		.vsync_start = VAC_WQHD + VFP,
		.vsync_end = VAC_WQHD + VFP + VSA,
		.vtotal = VAC_WQHD + VFP + VSA + VBP,
		.hskew = SDC_MFR,
	},
	//wqhd_sdc_90_mode
	{
		.clock = 438460,
		.hdisplay = HAC_WQHD,
		.hsync_start = HAC_WQHD + HFP,
		.hsync_end = HAC_WQHD + HFP + HSA,
		.htotal = HAC_WQHD + HFP + HSA + HBP + 2,
		.vdisplay = VAC_WQHD,
		.vsync_start = VAC_WQHD + VFP,
		.vsync_end = VAC_WQHD + VFP + VSA,
		.vtotal = VAC_WQHD + VFP + VSA + VBP,
		.hskew = SDC_MFR,
	},
	//fhd_sdc_60_mode
	{
		.clock = 168084,
		.hdisplay = HAC_FHD,
		.hsync_start = HAC_FHD + HFP,
		.hsync_end = HAC_FHD + HFP + HSA,
		.htotal = HAC_FHD + HFP + HSA + HBP,
		.vdisplay = VAC_FHD,
		.vsync_start = VAC_FHD + VFP,
		.vsync_end = VAC_FHD + VFP + VSA,
		.vtotal = VAC_FHD + VFP + VSA + VBP,
		.hskew = SDC_MFR,
	},
	//FHD_SDC120
	{
		.clock = 336168,
		.hdisplay = HAC_FHD,
		.hsync_start = HAC_FHD + HFP,
		.hsync_end = HAC_FHD + HFP + HSA,
		.htotal = HAC_FHD + HFP + HSA + HBP,
		.vdisplay = VAC_FHD,
		.vsync_start = VAC_FHD + VFP,
		.vsync_end = VAC_FHD + VFP + VSA,
		.vtotal = VAC_FHD + VFP + VSA + VBP,
		.hskew = SDC_MFR,
	},
	//FHD_SDC90
	{
		.clock = 252126,
		.hdisplay = HAC_FHD,
		.hsync_start = HAC_FHD + HFP,
		.hsync_end = HAC_FHD + HFP + HSA,
		.htotal = HAC_FHD + HFP + HSA + HBP + 2,
		.vdisplay = VAC_FHD,
		.vsync_start = VAC_FHD + VFP,
		.vsync_end = VAC_FHD + VFP + VSA,
		.vtotal = VAC_FHD + VFP + VSA + VBP,
		.hskew = SDC_MFR,
	},
};

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct lcm *ctx = panel_to_lcm(panel);

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

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0, 0, 0};
	unsigned char id[3] = {0x00, 0x80, 0x00};
	ssize_t ret;

	pr_info("%s success\n", __func__);

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0)
		pr_info("%s error\n", __func__);

	DDPINFO("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	DDPINFO("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 1;
}

static int panel_send_pack_hs_cmd(void *dsi, struct LCM_setting_table *table,
			unsigned int lcm_cmd_count, dcs_write_gce_pack cb, void *handle)
{
	unsigned int i = 0;
	struct mtk_ddic_dsi_cmd send_cmd_to_ddic;

	if (lcm_cmd_count > MAX_TX_CMD_NUM_PACK) {
		pr_info("debug for lcm %s,out of mtk_ddic_dsi_cmd\n", __func__);
		return 0;
	}

	for (i = 0; i < lcm_cmd_count; i++) {
		send_cmd_to_ddic.mtk_ddic_cmd_table[i].cmd_num = table[i].count;
		send_cmd_to_ddic.mtk_ddic_cmd_table[i].para_list = table[i].para_list;
	}
	send_cmd_to_ddic.is_hs = 1;
	send_cmd_to_ddic.is_package = 1;
	send_cmd_to_ddic.cmd_count = lcm_cmd_count;

	cb(dsi, handle, &send_cmd_to_ddic);

	return 0;
}

static int panel_doze_enable_start(struct drm_panel *panel, void *dsi, dcs_write_gce cb,
	void *handle)
{
	static char aod_en0[] = { 0xF0, 0x5A, 0x5A}, aod_en1[] = { 0x53, 0x24},
	aod_en2[] = { 0xF0, 0xA5, 0xA5}, aod_en3[] = { 0x29};

	pr_info("%s\n", __func__);

	if (!cb)
		return -1;

	cb(dsi, handle, aod_en0, ARRAY_SIZE(aod_en0));
	cb(dsi, handle, aod_en1, ARRAY_SIZE(aod_en1));
	cb(dsi, handle, aod_en2, ARRAY_SIZE(aod_en2));
	msleep(80);

	cb(dsi, handle, aod_en3, ARRAY_SIZE(aod_en3));

	return 0;
}

static int panel_doze_disable(struct drm_panel *panel, void *dsi, dcs_write_gce cb,
	void *handle)
{
	static char aod_en0[] = { 0xF0, 0x5A, 0x5A}, aod_en1[] = { 0x53, 0x28},
	aod_en2[] = { 0xF0, 0xA5, 0xA5};

	pr_info("%s\n", __func__);

	if (!cb)
		return -1;

	cb(dsi, handle, aod_en0, ARRAY_SIZE(aod_en0));
	cb(dsi, handle, aod_en1, ARRAY_SIZE(aod_en1));
	cb(dsi, handle, aod_en2, ARRAY_SIZE(aod_en2));

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	if (level < 0x06)
		level = 0x06;

	bl_tb0[1] = level >> 8;
	bl_tb0[2] = level & 0xFF;
	pr_info("%s bl_tb0[1] = 0x%x,bl_tb0[2]= 0x%x\n", __func__, bl_tb0[1], bl_tb0[2]);

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	return 0;
}
static struct mtk_panel_params ext_params[MODE_NUM] = {
	//WQHD_SDC60
	{
		.data_rate = 1372,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.dsc_params = {
			.enable = 1,
			.ver = 17,
			.slice_mode = 1,
			.rgb_swap = 0,
			.dsc_cfg = 2088,
			.rct_on = 1,
			.bit_per_channel = 10,
			.dsc_line_buf_depth = 11,
			.bp_enable = 1,
			.bit_per_pixel = 128,
			.pic_height = 3216,
			.pic_width = 1440,
			.slice_height = 24,
			.slice_width = 720,
			.chunk_size = 720,
			.xmit_delay = 512,
			.dec_delay = 646,
			.scale_value = 32,
			.increment_interval = 643,
			.decrement_interval = 10,
			.line_bpg_offset = 13,
			.nfl_bpg_offset = 1158,
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
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 60,
		},
		.cust_esd_check = 0,
		.esd_check_enable = 1,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0a,
			.count = 1,
			.para_list[0] = 0x9f,
		},
	},
	//WQHD_SDC120
	{
		.data_rate = 1372,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.dsc_params = {
			.enable = 1,
			.ver = 17,
			.slice_mode = 1,
			.rgb_swap = 0,
			.dsc_cfg = 2088,
			.rct_on = 1,
			.bit_per_channel = 10,
			.dsc_line_buf_depth = 11,
			.bp_enable = 1,
			.bit_per_pixel = 128,
			.pic_height = 3216,
			.pic_width = 1440,
			.slice_height = 24,
			.slice_width = 720,
			.chunk_size = 720,
			.xmit_delay = 512,
			.dec_delay = 646,
			.scale_value = 32,
			.increment_interval = 643,
			.decrement_interval = 10,
			.line_bpg_offset = 13,
			.nfl_bpg_offset = 1158,
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
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 120,
		},
		.cust_esd_check = 0,
		.esd_check_enable = 1,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0a,
			.count = 1,
			.para_list[0] = 0x9f,
		},
	},
	//WQHD_SDC90
	{
		.data_rate = 1372,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.dsc_params = {
			.enable = 1,
			.ver = 17,
			.slice_mode = 1,
			.rgb_swap = 0,
			.dsc_cfg = 2088,
			.rct_on = 1,
			.bit_per_channel = 10,
			.dsc_line_buf_depth = 11,
			.bp_enable = 1,
			.bit_per_pixel = 128,
			.pic_height = 3216,
			.pic_width = 1440,
			.slice_height = 24,
			.slice_width = 720,
			.chunk_size = 720,
			.xmit_delay = 512,
			.dec_delay = 646,
			.scale_value = 32,
			.increment_interval = 643,
			.decrement_interval = 10,
			.line_bpg_offset = 13,
			.nfl_bpg_offset = 1158,
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
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 90,
		},
		.cust_esd_check = 0,
		.esd_check_enable = 1,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0a,
			.count = 1,
			.para_list[0] = 0x9f,
		},
	},
	//FHD_SDC60
	{
		.data_rate = 826,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.dsc_params = {
			.enable = 1,
			.ver = 17,
			.slice_mode = 1,
			.rgb_swap = 0,
			.dsc_cfg = 2088,
			.rct_on = 1,
			.bit_per_channel = 10,
			.dsc_line_buf_depth = 11,
			.bp_enable = 1,
			.bit_per_pixel = 128,
			.pic_height = 2412,
			.pic_width = 1080,
			.slice_height = 36,
			.slice_width = 540,
			.chunk_size = 540,
			.xmit_delay = 512,
			.dec_delay = 571,
			.scale_value = 32,
			.increment_interval = 821,
			.decrement_interval = 7,
			.line_bpg_offset = 14,
			.nfl_bpg_offset = 820,
			.slice_bpg_offset = 724,
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
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 60,
		},
		.cust_esd_check = 0,
		.esd_check_enable = 1,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0a,
			.count = 1,
			.para_list[0] = 0x9f,
		},
	},
	//FHD_SDC120
	{
		.data_rate = 826,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.dsc_params = {
			.enable = 1,
			.ver = 17,
			.slice_mode = 1,
			.rgb_swap = 0,
			.dsc_cfg = 2088,
			.rct_on = 1,
			.bit_per_channel = 10,
			.dsc_line_buf_depth = 11,
			.bp_enable = 1,
			.bit_per_pixel = 128,
			.pic_height = 2412,
			.pic_width = 1080,
			.slice_height = 36,
			.slice_width = 540,
			.chunk_size = 540,
			.xmit_delay = 512,
			.dec_delay = 571,
			.scale_value = 32,
			.increment_interval = 821,
			.decrement_interval = 7,
			.line_bpg_offset = 14,
			.nfl_bpg_offset = 820,
			.slice_bpg_offset = 724,
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
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 120,
		},
		.cust_esd_check = 0,
		.esd_check_enable = 1,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0a,
			.count = 1,
			.para_list[0] = 0x9f,
		},
	},
	//FHD_SDC90
	{
		.data_rate = 826,
		.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
		.dsc_params = {
			.enable = 1,
			.ver = 17,
			.slice_mode = 1,
			.rgb_swap = 0,
			.dsc_cfg = 2088,
			.rct_on = 1,
			.bit_per_channel = 10,
			.dsc_line_buf_depth = 11,
			.bp_enable = 1,
			.bit_per_pixel = 128,
			.pic_height = 2412,
			.pic_width = 1080,
			.slice_height = 36,
			.slice_width = 540,
			.chunk_size = 540,
			.xmit_delay = 512,
			.dec_delay = 571,
			.scale_value = 32,
			.increment_interval = 821,
			.decrement_interval = 7,
			.line_bpg_offset = 14,
			.nfl_bpg_offset = 820,
			.slice_bpg_offset = 724,
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
		},
		.dyn_fps = {
			.switch_en = 1, .vact_timing_fps = 90,
		},
		.cust_esd_check = 0,
		.esd_check_enable = 1,
		.lcm_esd_check_table[0] = {
			.cmd = 0x0a,
			.count = 1,
			.para_list[0] = 0x9f,
		},
	},
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
	int mode_id = -1;

	if (!connector || !panel) {
		pr_info("%s, invalid param\n", __func__);
		return -1;
	}

	mode_id = get_mode_enum(get_mode_by_id(connector, mode));
	*ext_param = &ext_params[mode_id];

	if (*ext_param)
		DDPMSG("%s mode_id=%d, data_rate:%d\n", __func__, mode_id,
			(*ext_param)->data_rate);
	else
		DDPMSG("%s mode_id=%d, ext_param is null\n", __func__, mode_id);

	return 0;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
	struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int mode_id = -1;

	if (!connector || !panel) {
		pr_info("%s, invalid param\n", __func__);
		return -1;
	}

	mode_id = get_mode_enum(get_mode_by_id(connector, mode));
	DDPMSG("%s mode_id=%d\n", __func__, mode_id);
	ext->params = &ext_params[mode_id];

	return 0;
}

static int mode_switch_hs(struct drm_panel *panel, struct drm_connector *connector,
		void *dsi_drv, unsigned int cur_mode, unsigned int dst_mode,
		enum MTK_PANEL_MODE_SWITCH_STAGE stage, dcs_write_gce_pack cb)
{
	int ret = 1;
	unsigned int lcm_cmd_count = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *src_m = get_mode_by_id(connector, cur_mode);
	struct lcm *ctx = panel_to_lcm(panel);
	int mode_id = -1;

	if (cur_mode == dst_mode)
		return ret;

	if (m == NULL || src_m == NULL) {
		DDPPR_ERR("%s:%d invalid display_mode\n", __func__, __LINE__);
		return -1;
	}

	if (stage == BEFORE_DSI_POWERDOWN) {
		mode_id = get_mode_enum(m);
		DDPMSG("%s mode_id:%d->%d\n", __func__, get_mode_enum(src_m),
			mode_id);

		//if resolution changed, resend dsc params
		if (m->hdisplay == HAC_WQHD && src_m->hdisplay == HAC_FHD) {
			DDPMSG("wqhd dsc param\n");
			push_table(ctx, wqhd_dsc_cmd,
				sizeof(wqhd_dsc_cmd)
					/ sizeof(struct LCM_setting_table));
		} else if (m->hdisplay == HAC_FHD
				&& src_m->hdisplay == HAC_WQHD) {
			DDPMSG("fhd dsc param\n");
			push_table(ctx, fhd_dsc_cmd,
				sizeof(fhd_dsc_cmd)
					/ sizeof(struct LCM_setting_table));
		}

		//1.send pre-switch cmd in sdc90
		//2.set fakeframe off
		//3.switch TE signal to TPV/TE according Mode
		lcm_cmd_count = sizeof(pre_switch_cmd)
			/ sizeof(struct LCM_setting_table);
		panel_send_pack_hs_cmd(dsi_drv, pre_switch_cmd, lcm_cmd_count, cb, NULL);

		//4.send timing switch cmd
		switch (mode_id) {
		case WQHD_SDC60:
			lcm_cmd_count = sizeof(wqhd_timing_switch_1_cmd_sdc60)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, wqhd_timing_switch_1_cmd_sdc60,
				lcm_cmd_count, cb, NULL);
			lcm_cmd_count = sizeof(wqhd_timing_switch_2_cmd_sdc60)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, wqhd_timing_switch_2_cmd_sdc60,
				lcm_cmd_count, cb, NULL);

			break;
		case WQHD_SDC120:
			lcm_cmd_count = sizeof(wqhd_timing_switch_1_cmd_sdc120)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, wqhd_timing_switch_1_cmd_sdc120,
				lcm_cmd_count, cb, NULL);
			lcm_cmd_count = sizeof(wqhd_timing_switch_2_cmd_sdc120)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, wqhd_timing_switch_2_cmd_sdc120,
				lcm_cmd_count, cb, NULL);

			break;
		case WQHD_SDC90:
			lcm_cmd_count = sizeof(wqhd_timing_switch_1_cmd_sdc90)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, wqhd_timing_switch_1_cmd_sdc90,
				lcm_cmd_count, cb, NULL);
			lcm_cmd_count = sizeof(wqhd_timing_switch_2_cmd_sdc90)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, wqhd_timing_switch_2_cmd_sdc90,
				lcm_cmd_count, cb, NULL);

			break;
		case FHD_SDC60:
			lcm_cmd_count = sizeof(fhd_timing_switch_1_cmd_sdc60)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, fhd_timing_switch_1_cmd_sdc60,
				lcm_cmd_count, cb, NULL);
			lcm_cmd_count = sizeof(fhd_timing_switch_2_cmd_sdc60)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, fhd_timing_switch_2_cmd_sdc60,
				lcm_cmd_count, cb, NULL);

			break;
		case FHD_SDC120:
			lcm_cmd_count = sizeof(fhd_timing_switch_1_cmd_sdc120)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, fhd_timing_switch_1_cmd_sdc120,
				lcm_cmd_count, cb, NULL);
			lcm_cmd_count = sizeof(fhd_timing_switch_2_cmd_sdc120)
				/ sizeof(struct LCM_setting_table);
			push_table(ctx,
				fhd_timing_switch_2_cmd_sdc120, lcm_cmd_count);
			break;
		case FHD_SDC90:
			lcm_cmd_count = sizeof(fhd_timing_switch_1_cmd_sdc90)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, fhd_timing_switch_1_cmd_sdc90,
				lcm_cmd_count, cb, NULL);
			lcm_cmd_count = sizeof(fhd_timing_switch_2_cmd_sdc90)
				/ sizeof(struct LCM_setting_table);
			panel_send_pack_hs_cmd(dsi_drv, fhd_timing_switch_2_cmd_sdc90,
				lcm_cmd_count, cb, NULL);
			break;
		default:
			DDPMSG("%s: error mode_id %d\n", __func__, mode_id);
			break;
		}
	} else if (stage == AFTER_DSI_POWERON) {
		ctx->m = m;
	}

	return ret;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.doze_disable = panel_doze_disable,
	.doze_enable_start = panel_doze_enable_start,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	//.mode_switch = mode_switch,
	.mode_switch_hs = mode_switch_hs,
};
#endif

static int lcm_get_modes(struct drm_panel *panel,
		struct drm_connector *connector)
{
	struct drm_display_mode *mode[MODE_NUM];
	int i = 0;

	mode[0] = drm_mode_duplicate(connector->dev, &display_mode[0]);
	if (!mode[0]) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			display_mode[0].hdisplay, display_mode[0].vdisplay,
			drm_mode_vrefresh(&display_mode[0]));
		return -ENOMEM;
	}

	drm_mode_set_name(mode[0]);
	mode[0]->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode[0]);

	for (i = 1; i < MODE_NUM; i++) {
		mode[i] = drm_mode_duplicate(connector->dev, &display_mode[i]);
		if (!mode[i]) {
			dev_info(connector->dev->dev, "not enough memory\n");
			return -ENOMEM;
		}

		drm_mode_set_name(mode[i]);
		mode[i]->type = DRM_MODE_TYPE_DRIVER;
		drm_mode_probed_add(connector, mode[i]);
	}
	connector->display_info.width_mm = 70;
	connector->display_info.height_mm = 156;

	return MODE_NUM;
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

	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	g_pmic = kzalloc(sizeof(struct lcm_pmic_info), GFP_KERNEL);

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
		dev_info(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);

	ctx->te_switch_gpio = devm_gpiod_get(dev, "te_switch", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->te_switch_gpio)) {
		dev_info(dev, "%s: cannot get te_switch_gpio %ld\n",
			__func__, PTR_ERR(ctx->te_switch_gpio));
		return PTR_ERR(ctx->te_switch_gpio);
	}
	gpiod_set_value(ctx->te_switch_gpio, 1);
	devm_gpiod_put(dev, ctx->te_switch_gpio);


	ctx->te_out_gpio = devm_gpiod_get(dev, "te_out", GPIOD_IN);
	if (IS_ERR(ctx->te_out_gpio)) {
		dev_info(dev, "%s: cannot get te_out_gpio %ld\n",
			__func__, PTR_ERR(ctx->te_out_gpio));
		return PTR_ERR(ctx->te_out_gpio);
	}

	devm_gpiod_put(dev, ctx->te_out_gpio);
	ctx->vddr1p5_enable_gpio = devm_gpiod_get(dev, "vddr1p5-enable", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vddr1p5_enable_gpio)) {
		dev_info(dev, "%s: cannot get vddr1p5_enable_gpio %ld\n",
			__func__, PTR_ERR(ctx->vddr1p5_enable_gpio));
		return PTR_ERR(ctx->vddr1p5_enable_gpio);
	}
	devm_gpiod_put(dev, ctx->vddr1p5_enable_gpio);

	g_pmic->reg_vufs18 = regulator_get(dev, "1p8");
	if (IS_ERR(g_pmic->reg_vufs18)) {
		dev_info(dev, "%s[lh]: cannot get reg_vufs18 %ld\n",
			__func__, PTR_ERR(g_pmic->reg_vufs18));
	}
	ret = lcm_enable_reg_vufs18(1);

	g_pmic->reg_vmch3p0 = regulator_get(dev, "3p0");
	if (IS_ERR(g_pmic->reg_vmch3p0)) {
		dev_info(dev, "%s[lh]: cannot get reg_vmch3p0 %ld\n",
			__func__, PTR_ERR(g_pmic->reg_vmch3p0));
	}
	msleep(20);
	ret = lcm_enable_reg_vmch3p0(1);

#ifndef CONFIG_MTK_DISP_NO_LK
	ctx->prepared = true;
	ctx->enabled = true;
#endif

	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &lcm_drm_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params[0], &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
	pr_info("%s-\n", __func__);

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
	{ .compatible = "samsung,op,cmd", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "samsung_op_cmd",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("MEDIATEK");
MODULE_DESCRIPTION("Samsung ANA6705 AMOLED CMD LCD Panel Driver");
MODULE_LICENSE("GPL v2");
