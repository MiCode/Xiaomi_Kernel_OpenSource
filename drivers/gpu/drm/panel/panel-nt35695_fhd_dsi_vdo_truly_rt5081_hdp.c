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
#include <linux/of_address.h>
#include <linux/platform_device.h>

#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define HFP (40)
#define HSA (10)
#define HBP (20)
#define VFP (40)
#define VSA (2)
#define VBP (8)
#define VAC (1920)
#define HAC (1080)
static u32 fake_heigh = 1920;
static u32 fake_width = 1080;
static bool need_fake_resolution;

#define RESET_PIN_DISPSYS_REG
#if defined(RESET_PIN_DISPSYS_REG)
#define mt_reg_sync_writel(v, a) \
	do {    \
		__raw_writel((v), (void __force __iomem *)((a)));   \
		mb();  /*make sure register access in order */ \
	} while (0)

#define DISP_REG_SET(handle, reg32, val) \
	do { \
		if (handle == NULL) { \
			mt_reg_sync_writel(val, (unsigned long *)(reg32));\
		} \
	} while (0)

static void set_reset_pin(int value)
{
	unsigned long va = 0;
	struct device_node *node = NULL;

	if (!va) {
		node = of_find_node_by_name(NULL, "dispsys_config");
		if (node == NULL) {
			pr_info("[ERR]%s, unable to find node dispsys_config\n", __func__);
			return;
		}

		va = (unsigned long)of_iomap(node, 0);
		if (!va) {
			pr_info("[ERR]%s, unable to ge dispsys_config VA\n", __func__);
			return;
		}
	}
	DISP_REG_SET(NULL, va + 0x150, value);
}
#endif

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *bias_pos, *bias_neg;

	bool prepared;
	bool enabled;

	unsigned int gate_ic;

	int error;
};

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

#if IS_ENABLED(CONFIG_RT5081_PMU_DSV) || IS_ENABLED(CONFIG_REGULATOR_MT6370)
static struct regulator *disp_bias_pos;
static struct regulator *disp_bias_neg;

static int lcm_panel_bias_regulator_init(void)
{
	static int regulator_inited;
	int ret = 0;

	if (regulator_inited)
		return ret;

	/* please only get regulator once in a driver */
	disp_bias_pos = regulator_get(NULL, "dsv_pos");
	if (IS_ERR(disp_bias_pos)) { /* handle return value */
		ret = PTR_ERR(disp_bias_pos);
		pr_err("get dsv_pos fail, error: %d\n", ret);
		return ret;
	}

	disp_bias_neg = regulator_get(NULL, "dsv_neg");
	if (IS_ERR(disp_bias_neg)) { /* handle return value */
		ret = PTR_ERR(disp_bias_neg);
		pr_err("get dsv_neg fail, error: %d\n", ret);
		return ret;
	}

	regulator_inited = 1;
	return ret; /* must be 0 */

}

static int lcm_panel_bias_enable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	/* set voltage with min & max*/
	ret = regulator_set_voltage(disp_bias_pos, 5400000, 5400000);
	if (ret < 0)
		pr_err("set voltage disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_set_voltage(disp_bias_neg, 5400000, 5400000);
	if (ret < 0)
		pr_err("set voltage disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	/* enable regulator */
	ret = regulator_enable(disp_bias_pos);
	if (ret < 0)
		pr_err("enable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_enable(disp_bias_neg);
	if (ret < 0)
		pr_err("enable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}

static int lcm_panel_bias_disable(void)
{
	int ret = 0;
	int retval = 0;

	lcm_panel_bias_regulator_init();

	ret = regulator_disable(disp_bias_neg);
	if (ret < 0)
		pr_err("disable regulator disp_bias_neg fail, ret = %d\n", ret);
	retval |= ret;

	ret = regulator_disable(disp_bias_pos);
	if (ret < 0)
		pr_err("disable regulator disp_bias_pos fail, ret = %d\n", ret);
	retval |= ret;

	return retval;
}
#endif

static void lcm_mdelay(unsigned int ms)
{
	if (ms < 10)
		udelay(ms * 1000);
	else if (ms <= 20)
		usleep_range(ms*1000, (ms+1)*1000);
	else
		usleep_range(ms * 1000 - 100, ms * 1000);
}

static void lcm_panel_init(struct lcm *ctx)
{
#if defined(RESET_PIN_DISPSYS_REG)
	set_reset_pin(0);
	lcm_mdelay(15);
	set_reset_pin(1);
	lcm_mdelay(1);
	set_reset_pin(0);
	lcm_mdelay(10);
	set_reset_pin(1);
	lcm_mdelay(10);
#else
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return;
	}

	gpiod_set_value(ctx->reset_gpio, 0);
	lcm_mdelay(15);
	gpiod_set_value(ctx->reset_gpio, 1);
	lcm_mdelay(1);
	gpiod_set_value(ctx->reset_gpio, 0);
	lcm_mdelay(10);
	gpiod_set_value(ctx->reset_gpio, 1);
	lcm_mdelay(10);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
#endif
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x24);	/* Return  To      CMD1 */
	lcm_dcs_write_seq_static(ctx, 0x6E, 0x10);	/* Return  To      CMD1 */
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);	/* Return  To      CMD1 */
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);	/* Return  To      CMD1 */

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);	/* Return  To      CMD1 */
	udelay(1);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x03);/*VDO MODE*/
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x03, 0x0A, 0x0A, 0x0A, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x24);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5E, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x24);	/* CMD2 Page4 Entrance */
	udelay(1);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0xB0);
	lcm_dcs_write_seq_static(ctx, 0x72, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x93, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x94, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x9B, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0x33);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x39);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x1B);
	lcm_dcs_write_seq_static(ctx, 0x89, 0x39);
	lcm_dcs_write_seq_static(ctx, 0x8B, 0xF4);
	lcm_dcs_write_seq_static(ctx, 0x8C, 0x01);
	/* Change for 695 RTN Start */
	lcm_dcs_write_seq_static(ctx, 0x90, 0x95);
	lcm_dcs_write_seq_static(ctx, 0x91, 0xC8);
	/* modify to 0x77 to see whether fps is higher */
	/* {0x92,1,{0x79}}, */
	lcm_dcs_write_seq_static(ctx, 0x92, 0x95);
	lcm_dcs_write_seq_static(ctx, 0x93, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x94, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x2B);
	lcm_dcs_write_seq_static(ctx, 0x96, 0x95);
	/* Change for 695 RTN End */
	lcm_dcs_write_seq_static(ctx, 0xDE, 0xFF);
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x82);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x02, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x04, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x09, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x0A, 0X03);
	lcm_dcs_write_seq_static(ctx, 0x0B, 0X04);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x0D, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x0E, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0x17);
	lcm_dcs_write_seq_static(ctx, 0x10, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x12, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x13, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x14, 0x0B);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x16, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x17, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x19, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x1A, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x1B, 0X04);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x13);
	lcm_dcs_write_seq_static(ctx, 0x1E, 0x15);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x17);

	lcm_dcs_write_seq_static(ctx, 0x20, 0x09);
	lcm_dcs_write_seq_static(ctx, 0x21, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x22, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x23, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x24, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x6D);
	lcm_dcs_write_seq_static(ctx, 0x26, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x27, 0x00);

	lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x30, 0x04);
	lcm_dcs_write_seq_static(ctx, 0x31, 0x49);
	lcm_dcs_write_seq_static(ctx, 0x32, 0x23);
	lcm_dcs_write_seq_static(ctx, 0x33, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x34, 0x00);
	/* Change for 695 */
	lcm_dcs_write_seq_static(ctx, 0x35, 0x83);
	lcm_dcs_write_seq_static(ctx, 0x36, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x37, 0x2D);
	lcm_dcs_write_seq_static(ctx, 0x38, 0x08);
	lcm_dcs_write_seq_static(ctx, 0x39, 0x00);
	/* Change for 695 */
	lcm_dcs_write_seq_static(ctx, 0x3A, 0x83);

	lcm_dcs_write_seq_static(ctx, 0x29, 0x58);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x16);

	lcm_dcs_write_seq_static(ctx, 0x5B, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5F, 0x75);
	lcm_dcs_write_seq_static(ctx, 0x63, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x67, 0x04);

	/* Change for 695 */
	lcm_dcs_write_seq_static(ctx, 0x74, 0x14);
	/* Change for 695 */
	lcm_dcs_write_seq_static(ctx, 0x75, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0x7B, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x7C, 0xD8);
	lcm_dcs_write_seq_static(ctx, 0x7D, 0x60);
	/* Change for 695 */
	lcm_dcs_write_seq_static(ctx, 0x7E, 0x14);
	/* Change for 695 */
	lcm_dcs_write_seq_static(ctx, 0x7F, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0x80, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x82, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x07);
	lcm_dcs_write_seq_static(ctx, 0x74, 0x10);
	lcm_dcs_write_seq_static(ctx, 0x75, 0x19);
	lcm_dcs_write_seq_static(ctx, 0x76, 0x06);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x03);

	lcm_dcs_write_seq_static(ctx, 0x78, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x99, 0x33);
	lcm_dcs_write_seq_static(ctx, 0x98, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x28);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x10);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x20);	/* Page    0,1,{   power-related   setting */
	udelay(1);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x01, 0x55);
	lcm_dcs_write_seq_static(ctx, 0x02, 0x45);
	lcm_dcs_write_seq_static(ctx, 0x03, 0x55);
	lcm_dcs_write_seq_static(ctx, 0x05, 0x50);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x9E);
	lcm_dcs_write_seq_static(ctx, 0x07, 0xA8);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x0C);
	lcm_dcs_write_seq_static(ctx, 0x0B, 0x96);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x96);
	lcm_dcs_write_seq_static(ctx, 0x0E, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x29);
	lcm_dcs_write_seq_static(ctx, 0x12, 0x29);
	lcm_dcs_write_seq_static(ctx, 0x13, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x14, 0x0A);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x99);
	lcm_dcs_write_seq_static(ctx, 0x16, 0x99);
	/* Change for 695 */
	lcm_dcs_write_seq_static(ctx, 0x6D, 0x68);
	lcm_dcs_write_seq_static(ctx, 0x58, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x59, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x5B, 0x05);
	lcm_dcs_write_seq_static(ctx, 0x5C, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5E, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x5F, 0x00);

	lcm_dcs_write_seq_static(ctx, 0x1B, 0x39);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x39);
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x47);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x20);	/* Page    0,1,{   power-related   setting */
	udelay(1);
	/* R+      ,1,{}}, */
	lcm_dcs_write_seq_static(ctx, 0x75, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x76, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x78, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x7A, 0x6f);
	lcm_dcs_write_seq_static(ctx, 0x7B, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x7C, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0x7D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x7E, 0xa7);
	lcm_dcs_write_seq_static(ctx, 0x7F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x80, 0xbc);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x82, 0xce);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x84, 0xdd);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x86, 0xec);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x1a);
	lcm_dcs_write_seq_static(ctx, 0x89, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0x8B, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x8C, 0x76);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x8E, 0xa3);
	lcm_dcs_write_seq_static(ctx, 0x8F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x90, 0xe9);
	lcm_dcs_write_seq_static(ctx, 0x91, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x92, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x93, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x94, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x96, 0x56);
	lcm_dcs_write_seq_static(ctx, 0x97, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x98, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0x99, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x9A, 0xb2);
	lcm_dcs_write_seq_static(ctx, 0x9B, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0xe2);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xA0, 0x2d);
	lcm_dcs_write_seq_static(ctx, 0xA2, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xA3, 0x3a);
	lcm_dcs_write_seq_static(ctx, 0xA4, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xA5, 0x48);
	lcm_dcs_write_seq_static(ctx, 0xA6, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xA7, 0x58);
	lcm_dcs_write_seq_static(ctx, 0xA9, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xAA, 0x6a);
	lcm_dcs_write_seq_static(ctx, 0xAB, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xAC, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xAD, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xAE, 0x9d);
	lcm_dcs_write_seq_static(ctx, 0xAF, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0xb7);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0xFF);
	/* R-      ,1,{}}, */
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB6, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB8, 0x6f);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xBC, 0xa7);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xBE, 0xbc);
	lcm_dcs_write_seq_static(ctx, 0xBF, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0xce);
	lcm_dcs_write_seq_static(ctx, 0xC1, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xC2, 0xdd);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xC4, 0xec);
	lcm_dcs_write_seq_static(ctx, 0xC5, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xC6, 0x1a);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xC8, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0xC9, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0x76);
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xCC, 0xa3);
	lcm_dcs_write_seq_static(ctx, 0xCD, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xCE, 0xe9);
	lcm_dcs_write_seq_static(ctx, 0xCF, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD0, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xD1, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD2, 0x22);
	lcm_dcs_write_seq_static(ctx, 0xD3, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD4, 0x56);
	lcm_dcs_write_seq_static(ctx, 0xD5, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD6, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0xD7, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD8, 0xb2);
	lcm_dcs_write_seq_static(ctx, 0xD9, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xDA, 0xe2);
	lcm_dcs_write_seq_static(ctx, 0xDB, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xDC, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xDD, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xDE, 0x2d);
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE0, 0x3a);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE2, 0x48);
	lcm_dcs_write_seq_static(ctx, 0xE3, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE4, 0x58);
	lcm_dcs_write_seq_static(ctx, 0xE5, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE6, 0x6a);
	lcm_dcs_write_seq_static(ctx, 0xE7, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE8, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0x9d);
	lcm_dcs_write_seq_static(ctx, 0xEB, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xEC, 0xb7);
	lcm_dcs_write_seq_static(ctx, 0xED, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xEE, 0xFF);
	/* G+      ,1,{}}, */
	lcm_dcs_write_seq_static(ctx, 0xEF, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF2, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0xF3, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x6f);
	lcm_dcs_write_seq_static(ctx, 0xF5, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF6, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0xF7, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xF8, 0xa7);
	lcm_dcs_write_seq_static(ctx, 0xF9, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFA, 0xbc);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x21);	/* Page    0,1,{   power-related   setting */
	udelay(1);
	lcm_dcs_write_seq_static(ctx, 0x00, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x01, 0xce);
	lcm_dcs_write_seq_static(ctx, 0x02, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x03, 0xdd);
	lcm_dcs_write_seq_static(ctx, 0x04, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x05, 0xec);
	lcm_dcs_write_seq_static(ctx, 0x06, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x07, 0x1a);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x09, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0x0A, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x0B, 0x76);
	lcm_dcs_write_seq_static(ctx, 0x0C, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x0D, 0xa3);
	lcm_dcs_write_seq_static(ctx, 0x0E, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x0F, 0xe9);
	lcm_dcs_write_seq_static(ctx, 0x10, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x11, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x12, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x13, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x14, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x15, 0x56);
	lcm_dcs_write_seq_static(ctx, 0x16, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x17, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0x18, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x19, 0xb2);
	lcm_dcs_write_seq_static(ctx, 0x1A, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x1B, 0xe2);
	lcm_dcs_write_seq_static(ctx, 0x1C, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x1D, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x1E, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x1F, 0x2d);
	lcm_dcs_write_seq_static(ctx, 0x20, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x21, 0x3a);
	lcm_dcs_write_seq_static(ctx, 0x22, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x23, 0x48);
	lcm_dcs_write_seq_static(ctx, 0x24, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x25, 0x58);
	lcm_dcs_write_seq_static(ctx, 0x26, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x27, 0x6a);
	lcm_dcs_write_seq_static(ctx, 0x28, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x29, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x9d);
	lcm_dcs_write_seq_static(ctx, 0x2D, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0xb7);
	lcm_dcs_write_seq_static(ctx, 0x30, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x31, 0xFF);
	/* G-      ,1,{}}, */
	lcm_dcs_write_seq_static(ctx, 0x32, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x33, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x34, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0x36, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x37, 0x6f);
	lcm_dcs_write_seq_static(ctx, 0x38, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x39, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0x3A, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0xa7);
	lcm_dcs_write_seq_static(ctx, 0x3D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x3F, 0xbc);
	lcm_dcs_write_seq_static(ctx, 0x40, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x41, 0xce);
	lcm_dcs_write_seq_static(ctx, 0x42, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x43, 0xdd);
	lcm_dcs_write_seq_static(ctx, 0x44, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x45, 0xec);
	lcm_dcs_write_seq_static(ctx, 0x46, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x47, 0x1a);
	lcm_dcs_write_seq_static(ctx, 0x48, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x49, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0x4A, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x4B, 0x76);
	lcm_dcs_write_seq_static(ctx, 0x4C, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x4D, 0xa3);
	lcm_dcs_write_seq_static(ctx, 0x4E, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x4F, 0xe9);
	lcm_dcs_write_seq_static(ctx, 0x50, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x52, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x53, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x54, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x55, 0x56);
	lcm_dcs_write_seq_static(ctx, 0x56, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x58, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0x59, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x5A, 0xb2);
	lcm_dcs_write_seq_static(ctx, 0x5B, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x5C, 0xe2);
	lcm_dcs_write_seq_static(ctx, 0x5D, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x5E, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x5F, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x60, 0x2d);
	lcm_dcs_write_seq_static(ctx, 0x61, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x62, 0x3a);
	lcm_dcs_write_seq_static(ctx, 0x63, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x64, 0x48);
	lcm_dcs_write_seq_static(ctx, 0x65, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x66, 0x58);
	lcm_dcs_write_seq_static(ctx, 0x67, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x68, 0x6a);
	lcm_dcs_write_seq_static(ctx, 0x69, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x6A, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x6B, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x6C, 0x9d);
	lcm_dcs_write_seq_static(ctx, 0x6D, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x6E, 0xb7);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x70, 0xFF);
	/* B+      ,1,{}}, */
	lcm_dcs_write_seq_static(ctx, 0x71, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x72, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x73, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x74, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0x75, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x76, 0x6f);
	lcm_dcs_write_seq_static(ctx, 0x77, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x78, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0x79, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x7A, 0xa7);
	lcm_dcs_write_seq_static(ctx, 0x7B, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x7C, 0xbc);
	lcm_dcs_write_seq_static(ctx, 0x7D, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x7E, 0xce);
	lcm_dcs_write_seq_static(ctx, 0x7F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x80, 0xdd);
	lcm_dcs_write_seq_static(ctx, 0x81, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x82, 0xec);
	lcm_dcs_write_seq_static(ctx, 0x83, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x84, 0x1a);
	lcm_dcs_write_seq_static(ctx, 0x85, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x86, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0x87, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x88, 0x76);
	lcm_dcs_write_seq_static(ctx, 0x89, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x8A, 0xa3);
	lcm_dcs_write_seq_static(ctx, 0x8B, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x8C, 0xe9);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x8E, 0x20);
	lcm_dcs_write_seq_static(ctx, 0x8F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x90, 0x22);
	lcm_dcs_write_seq_static(ctx, 0x91, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x92, 0x56);
	lcm_dcs_write_seq_static(ctx, 0x93, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x94, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x96, 0xb2);
	lcm_dcs_write_seq_static(ctx, 0x97, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x98, 0xe2);
	lcm_dcs_write_seq_static(ctx, 0x99, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x9A, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x9B, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x9C, 0x2d);
	lcm_dcs_write_seq_static(ctx, 0x9D, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x9E, 0x3a);
	lcm_dcs_write_seq_static(ctx, 0x9F, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xA0, 0x48);
	lcm_dcs_write_seq_static(ctx, 0xA2, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xA3, 0x58);
	lcm_dcs_write_seq_static(ctx, 0xA4, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xA5, 0x6a);
	lcm_dcs_write_seq_static(ctx, 0xA6, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xA7, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xA9, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xAA, 0x9d);
	lcm_dcs_write_seq_static(ctx, 0xAB, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xAC, 0xb7);
	lcm_dcs_write_seq_static(ctx, 0xAD, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xAE, 0xFF);
	/* B-      ,1,{}}, */
	lcm_dcs_write_seq_static(ctx, 0xAF, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB0, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB1, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB2, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0xB3, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB4, 0x6f);
	lcm_dcs_write_seq_static(ctx, 0xB5, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB6, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0xB7, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xB8, 0xa7);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0xbc);
	lcm_dcs_write_seq_static(ctx, 0xBB, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xBC, 0xce);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xBE, 0xdd);
	lcm_dcs_write_seq_static(ctx, 0xBF, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xC0, 0xec);
	lcm_dcs_write_seq_static(ctx, 0xC1, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xC2, 0x1a);
	lcm_dcs_write_seq_static(ctx, 0xC3, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xC4, 0x3e);
	lcm_dcs_write_seq_static(ctx, 0xC5, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xC6, 0x76);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xC8, 0xa3);
	lcm_dcs_write_seq_static(ctx, 0xC9, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xCA, 0xe9);
	lcm_dcs_write_seq_static(ctx, 0xCB, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xCC, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xCD, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xCE, 0x22);
	lcm_dcs_write_seq_static(ctx, 0xCF, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD0, 0x56);
	lcm_dcs_write_seq_static(ctx, 0xD1, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD2, 0x8f);
	lcm_dcs_write_seq_static(ctx, 0xD3, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD4, 0xb2);
	lcm_dcs_write_seq_static(ctx, 0xD5, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD6, 0xe2);
	lcm_dcs_write_seq_static(ctx, 0xD7, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xD8, 0x02);
	lcm_dcs_write_seq_static(ctx, 0xD9, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xDA, 0x2d);
	lcm_dcs_write_seq_static(ctx, 0xDB, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xDC, 0x3a);
	lcm_dcs_write_seq_static(ctx, 0xDD, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xDE, 0x48);
	lcm_dcs_write_seq_static(ctx, 0xDF, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE0, 0x58);
	lcm_dcs_write_seq_static(ctx, 0xE1, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE2, 0x6a);
	lcm_dcs_write_seq_static(ctx, 0xE3, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE4, 0x80);
	lcm_dcs_write_seq_static(ctx, 0xE5, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE6, 0x9d);
	lcm_dcs_write_seq_static(ctx, 0xE7, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xE8, 0xb7);
	lcm_dcs_write_seq_static(ctx, 0xE9, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xEA, 0xFF);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x21);	/* Page    ,1,{    Gamma   Default Update */
	udelay(1);
	lcm_dcs_write_seq_static(ctx, 0xEB, 0x30);
	lcm_dcs_write_seq_static(ctx, 0xEC, 0x17);
	lcm_dcs_write_seq_static(ctx, 0xED, 0x20);
	lcm_dcs_write_seq_static(ctx, 0xEE, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xEF, 0x1F);
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xF1, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xF2, 0x07);

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x23);	/* CMD2    Page    3       Entrance */
	udelay(1);
	lcm_dcs_write_seq_static(ctx, 0x08, 0x04);

	/* image.first */
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10);	/* Return  To CMD1 */
	udelay(1);
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x44, 0x05, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x11);
	lcm_mdelay(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	/* {0x51,1,{0xFF}},//writedisplay brightness */

	lcm_dcs_write_seq_static(ctx, 0xFF, 0x24);
	lcm_dcs_write_seq_static(ctx, 0xC2, 0x00);
	lcm_dcs_write_seq_static(ctx, 0xFB, 0x01);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0x10); /* Return  To CMD1 */
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
	return 0;
	lcm_dcs_write_seq_static(ctx, 0x28);
	lcm_dcs_write_seq_static(ctx, 0x10);
	lcm_mdelay(80);
	lcm_dcs_write_seq_static(ctx, 0x4f, 0x01);
	lcm_mdelay(80);
	lcm_mdelay(10);
	ctx->error = 0;
	ctx->prepared = false;

#if defined(RESET_PIN_DISPSYS_REG)
	// set_reset_pin(0);
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
#if IS_ENABLED(CONFIG_RT5081_PMU_DSV) || IS_ENABLED(CONFIG_REGULATOR_MT6370)
	lcm_panel_bias_disable();
#else
	if (ctx->gate_ic == 0) {
		ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
			"bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);

		lcm_mdelay(1);

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
			"bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 0);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);
	}
#endif

	return 0;
}

static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	int ret;

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;

#if defined(RESET_PIN_DISPSYS_REG)
	set_reset_pin(0);
#else
	ctx->reset_gpio =
		devm_gpiod_get(ctx->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
			dev_err(ctx->dev, "%s: cannot get reset_gpio %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
			return -1;
	}
	gpiod_set_value(ctx->reset_gpio, 0);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);
#endif
#if IS_ENABLED(CONFIG_RT5081_PMU_DSV) || IS_ENABLED(CONFIG_REGULATOR_MT6370)
	lcm_panel_bias_enable();
#else
	if (ctx->gate_ic == 0) {

		ctx->bias_pos = devm_gpiod_get_index(ctx->dev,
			"bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_err(ctx->dev, "%s: cannot get bias_pos %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		gpiod_set_value(ctx->bias_pos, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_pos);

		lcm_mdelay(2);

		ctx->bias_neg = devm_gpiod_get_index(ctx->dev,
			"bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_err(ctx->dev, "%s: cannot get bias_neg %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		gpiod_set_value(ctx->bias_neg, 1);
		devm_gpiod_put(ctx->dev, ctx->bias_neg);
	}
#endif
	lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0)
		lcm_unprepare(panel);

	ctx->prepared = true;

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

static struct drm_display_mode default_mode = {
	.clock = 70626,
	.hdisplay = HAC,
	.hsync_start = HAC + HFP,
	.hsync_end = HAC + HFP + HSA,
	.htotal = HAC + HFP + HSA + HBP,
	.vdisplay = VAC,
	.vsync_start = VAC + VFP,
	.vsync_end = VAC + VFP + VSA,
	.vtotal = VAC + VFP + VSA + VBP,
	//.vrefresh = 60,
};

static void change_drm_disp_mode_params(struct drm_display_mode *mode)
{
	if (fake_heigh > 0 && fake_heigh < VAC) {
		mode->vdisplay = fake_heigh;
		mode->vsync_start = fake_heigh + VFP;
		mode->vsync_end = fake_heigh + VFP + VSA;
		mode->vtotal = fake_heigh + VFP + VSA + VBP;
	}
	if (fake_width > 0 && fake_width < HAC) {
		mode->hdisplay = fake_width;
		mode->hsync_start = fake_width + HFP;
		mode->hsync_end = fake_width + HFP + HSA;
		mode->htotal = fake_width + HFP + HSA + HBP;
	}
}

#if defined(CONFIG_MTK_PANEL_EXT)
static int panel_ext_reset(struct drm_panel *panel, int on)
{
#if defined(RESET_PIN_DISPSYS_REG)
	set_reset_pin(on);
#else
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
#endif
	return 0;
}

static int panel_ata_check(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x00, 0x00, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_err("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0] &&
			data[1] == id[1] &&
			data[2] == id[2])
		return 1;

	pr_info("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);

	return 0;
}

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb0[] = {0x51, 0xFF};

	bl_tb0[1] = level;

	if (!cb)
		return -1;

	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));

	return 0;
}

static int lcm_get_virtual_heigh(void)
{
	return VAC;
}

static int lcm_get_virtual_width(void)
{
	return HAC;
}

static struct mtk_panel_params ext_params = {
	.pll_clk = 440,
	.data_rate = 880,
	.vfp_low_power = 620,
	.cust_esd_check = 0,
	.esd_check_enable = 1,
	.lcm_esd_check_table[0] = {
		.cmd = 0x53,
		.count = 1,
		.para_list[0] = 0x24,
	},
};

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check = panel_ata_check,
	.get_virtual_heigh = lcm_get_virtual_heigh,
	.get_virtual_width = lcm_get_virtual_width,
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
	struct drm_display_mode *mode;

	if (need_fake_resolution)
		change_drm_disp_mode_params(&default_mode);
	mode = drm_mode_duplicate(connector->dev, &default_mode);
	if (!mode) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			default_mode.hdisplay, default_mode.vdisplay,
			drm_mode_vrefresh(&default_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

	return 1;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
};

static void check_is_need_fake_resolution(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "fake_heigh", &fake_heigh);
	if (ret) {
		need_fake_resolution = false;
		return;
	}
	ret = of_property_read_u32(dev->of_node, "fake_width", &fake_width);
	if (ret) {
		need_fake_resolution = false;
		return;
	}
	if (fake_heigh > 0 && fake_heigh < VAC && fake_width > 0 && fake_width < HAC)
		need_fake_resolution = true;
	else
		need_fake_resolution = false;
}

static int lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct lcm *ctx;
	struct device_node *backlight;
	unsigned int value;
	int ret;

	pr_info("%s+\n", __func__);
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
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO
			| MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
			 | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = of_property_read_u32(dev->of_node, "gate-ic", &value);
	if (ret < 0)
		value = 0;
	else
		ctx->gate_ic = value;

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx->backlight)
			return -EPROBE_DEFER;
	}
#if defined(RESET_PIN_DISPSYS_REG)
	// set_reset_pin(0);
#else
	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "%s: cannot get reset-gpios %ld\n",
			__func__, PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}
	devm_gpiod_put(dev, ctx->reset_gpio);
#endif
#ifndef CONFIG_RT4831A_I2C
#if IS_ENABLED(CONFIG_RT5081_PMU_DSV) || IS_ENABLED(CONFIG_REGULATOR_MT6370)
	lcm_panel_bias_enable();
#else
	if (ctx->gate_ic == 0) {
		ctx->bias_pos = devm_gpiod_get_index(dev, "bias", 0, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_pos)) {
			dev_err(dev, "%s: cannot get bias-pos 0 %ld\n",
				__func__, PTR_ERR(ctx->bias_pos));
			return PTR_ERR(ctx->bias_pos);
		}
		devm_gpiod_put(dev, ctx->bias_pos);

		ctx->bias_neg = devm_gpiod_get_index(dev, "bias", 1, GPIOD_OUT_HIGH);
		if (IS_ERR(ctx->bias_neg)) {
			dev_err(dev, "%s: cannot get bias-neg 1 %ld\n",
				__func__, PTR_ERR(ctx->bias_neg));
			return PTR_ERR(ctx->bias_neg);
		}
		devm_gpiod_put(dev, ctx->bias_neg);
	}
#endif
#endif
	ctx->prepared = true;
	ctx->enabled = true;

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
	check_is_need_fake_resolution(dev);
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
	{ .compatible = "nt35695,fhd,vdo,truly,rt5081,hdp", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "panel-nt35695-fhd-truly-vdo-hdp",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

MODULE_AUTHOR("Xiuhai Deng <xiuhai.deng@mediatek.com>");
MODULE_DESCRIPTION("nt35695 truly VDO LCD Panel Driver");
MODULE_LICENSE("GPL v2");
