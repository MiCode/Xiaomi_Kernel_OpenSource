
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
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#include "../mediatek/mediatek_v2/mtk_log.h"
#endif
#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif
#include <linux/atomic.h>
#define DATA_RATE1		800
#define MODE1_FPS		90
#define MODE1_HFP		60
#define MODE1_HSA		12
#define MODE1_HBP		40
#define MODE1_VFP		10
#define MODE1_VSA		2
#define MODE1_VBP		256
#define MODE0_FPS		60
#define MODE0_HFP		60
#define MODE0_HSA		12
#define MODE0_HBP		40
#define MODE0_VFP		10
#define MODE0_VSA		2
#define MODE0_VBP		256
#define FRAME_WIDTH		(720)
#define	FRAME_HEIGHT		(1650)
static int current_fps = 60;
struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *enp_gpio;
	struct gpio_desc *enn_gpio;
	bool prepared;
	bool enabled;
	unsigned int dynamic_fps;
	int error;
};
struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[200];
};
static struct lcm *panel_ctx;
#define lcm_dcs_write_seq(ctx, seq...) \
({\
	const u8 d[] = { seq };\
	BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64, "sequence too big for stack");\
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
#define REGFLAG_DELAY       0xFFFC
#define REGFLAG_UDELAY  0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW   0xFFFE
#define REGFLAG_RESET_HIGH  0xFFFF
static struct LCM_setting_table init_setting[] = {
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
	if (!ctx->prepared)
		return 0;
	pr_err("%s: +\n", __func__);
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(20);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(120);
	ctx->error = 0;
	ctx->prepared = false;
	ctx->enn_gpio =
		devm_gpiod_get(ctx->dev, "enn", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enn_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enn_gpio %ld\n",
			__func__, PTR_ERR(ctx->enn_gpio));
		return PTR_ERR(ctx->enn_gpio);
	}
	gpiod_set_value(ctx->enn_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->enn_gpio);
	udelay(2000);
	ctx->enp_gpio =
		devm_gpiod_get(ctx->dev, "enp", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enp_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enp_gpio %ld\n",
			__func__, PTR_ERR(ctx->enp_gpio));
		return PTR_ERR(ctx->enp_gpio);
	}
	gpiod_set_value(ctx->enp_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->enp_gpio);
	udelay(5000);
#if 1
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
	pr_err("%s: -\n", __func__);
	return 0;
}
static void lcm_panel_init(struct lcm *ctx)
{
	pr_err("%s: +\n", __func__);
	if (ctx->prepared) {
		pr_err("%s: panel has been prepared, nothing to do!\n", __func__);
		goto err;
	}
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		goto err;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(10 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(15 * 1000);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
	push_table(ctx, init_setting,
		sizeof(init_setting) / sizeof(struct LCM_setting_table));
	ctx->prepared = true;
err:
	pr_err("%s: -\n", __func__);
}
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;
	pr_err("%s\n", __func__);
	if (ctx->prepared)
		return 0;
	ctx->enp_gpio =
		devm_gpiod_get(ctx->dev, "enp", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enp_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enp_gpio %ld\n",
			__func__, PTR_ERR(ctx->enp_gpio));
		return PTR_ERR(ctx->enp_gpio);
	}
	gpiod_set_value(ctx->enp_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->enp_gpio);
	udelay(3000);
	ctx->enn_gpio =
		devm_gpiod_get(ctx->dev, "enn", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enn_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enn_gpio %ld\n",
			__func__, PTR_ERR(ctx->enn_gpio));
		return PTR_ERR(ctx->enn_gpio);
	}
	gpiod_set_value(ctx->enn_gpio, 1);
	devm_gpiod_put(ctx->dev, ctx->enn_gpio);
	udelay(5 * 1000);
	lcm_panel_init(ctx);
	ret = ctx->error;
	if (ret < 0){
		lcm_unprepare(panel);
		pr_err("%s error lcm_unprepare\n", __func__);
	}
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
static const struct drm_display_mode mode_60hz = {
	.clock = 552667,//h_total * v_total * fps 60hz
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE0_HFP,
	.hsync_end = FRAME_WIDTH + MODE0_HFP + MODE0_HSA,
	.htotal = FRAME_WIDTH + MODE0_HFP + MODE0_HSA + MODE0_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE0_VFP + MODE0_VSA,
	.vtotal = FRAME_HEIGHT + MODE0_VFP + MODE0_VSA + MODE0_VBP,
};
static const struct drm_display_mode default_mode = {
	.clock = 144133,//h_total * v_total * fps 90hz
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE1_HFP,
	.hsync_end = FRAME_WIDTH + MODE1_HFP + MODE1_HSA,
	.htotal = FRAME_WIDTH + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE1_VFP + MODE1_VSA,
	.vtotal = FRAME_HEIGHT + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};
#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = DATA_RATE1 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	//.is_support_od = true,
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	//.physical_width_um = 69540,
	//.physical_height_um = 154584,
	.data_rate = DATA_RATE1,
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
#endif
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
	else if (dst_fps == MODE1_FPS)
		*ext_param = &ext_params;
	else {
		pr_err("%s, dst_fps %d\n", __func__, dst_fps);
		ret = -EINVAL;
	}
	pr_err("%s, lcm get dst_fps %d\n", __func__, dst_fps);
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
	else if (dst_fps == MODE1_FPS)
			ext->params = &ext_params;
		else {
			pr_err("%s, dst_fps %d\n", __func__, dst_fps);
			ret = -EINVAL;
		}
	if (!ret)
		current_fps = dst_fps;
	return ret;
}
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb[] = {0x51, 0x07, 0xff};
	bl_tb[1] = (level >> 8) & 0xFF;
	bl_tb[2] = level & 0xFF;
	if (!cb)
		return -1;
	pr_err("%s %d %d %d\n", __func__, level, bl_tb[1], bl_tb[2]);
	cb(dsi, handle, bl_tb, ARRAY_SIZE(bl_tb));
	return 0;
}
static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_err("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		ctx->dynamic_fps = 90;
	}
}
static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_err("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
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
	if (cur_mode == dst_mode)
		return ret;
	dst_fps = m_dst ? drm_mode_vrefresh(m_dst) : -EINVAL;
	dst_vdisplay = m_dst ? m_dst->vdisplay : -EINVAL;
	dst_hdisplay = m_dst ? m_dst->hdisplay : -EINVAL;
	cur_fps = m_cur ? drm_mode_vrefresh(m_cur) : -EINVAL;
	cur_vdisplay = m_cur ? m_cur->vdisplay : -EINVAL;
	cur_hdisplay = m_cur ? m_cur->hdisplay : -EINVAL;
	isFpsChange = ((dst_fps == cur_fps) && (dst_fps != -EINVAL)
			&& (cur_fps != -EINVAL)) ? false : true;
	pr_err("%s isFpsChange = %d\n", __func__, isFpsChange);
	pr_err("%s dst_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, dst_fps, dst_vdisplay, dst_hdisplay);
	pr_err("%s cur_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, cur_fps, cur_vdisplay, cur_hdisplay);
	if (isFpsChange) {
		if (dst_fps == MODE0_FPS)
			mode_switch_to_60(panel, stage);
		else if (dst_fps == MODE1_FPS)
			mode_switch_to_90(panel, stage);
		else
			ret = 1;
	}
	return ret;
}
static struct mtk_panel_funcs ext_funcs = {
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.mode_switch = mode_switch,
};
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
	struct drm_display_mode *mode0, *mode1;
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
	mode1 = drm_mode_duplicate(connector->dev, &mode_60hz);
	if (!mode1) {
		dev_err(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			mode_60hz.hdisplay, mode_60hz.vdisplay,
			drm_mode_vrefresh(&mode_60hz));
		return -ENOMEM;
	}
	drm_mode_set_name(mode1);
	mode1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode1);
	connector->display_info.width_mm = 68;
	connector->display_info.height_mm = 156;
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
	pr_err("%s dsi_panel_c3y_null_hd_vdo +\n", __func__);
	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_err("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_err("device node name:%s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_err("%s+ skip probe due to not current lcm\n", __func__);
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
			| MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
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
	ctx->enp_gpio =
		devm_gpiod_get(ctx->dev, "enp", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enp_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enp_gpio %ld\n",
			__func__, PTR_ERR(ctx->enp_gpio));
		return PTR_ERR(ctx->enp_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->enp_gpio);
	ctx->enn_gpio =
		devm_gpiod_get(ctx->dev, "enn", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->enn_gpio)) {
		dev_err(ctx->dev, "%s: cannot get enn_gpio %ld\n",
			__func__, PTR_ERR(ctx->enn_gpio));
		return PTR_ERR(ctx->enn_gpio);
	}
	devm_gpiod_put(ctx->dev, ctx->enn_gpio);
	ctx->prepared = true;
	ctx->enabled = true;
	ctx->dynamic_fps = 90;
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
	pr_err("%s- dsi_panel_c3y_null_hd_vdo\n", __func__);
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
	{ .compatible = "dsi_panel_c3y_null_hd_vdo,lcm", },
	{ }
};
MODULE_DEVICE_TABLE(of, lcm_of_match);
static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "dsi_panel_c3y_null_hd_vdo",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};
module_mipi_dsi_driver(lcm_driver);
MODULE_DESCRIPTION("dsi_panel_c3y_null_hd_vdo panel driver");
MODULE_LICENSE("GPL v2");
