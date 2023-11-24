// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/backlight.h>
#include <linux/delay.h>
#include <drm/drm_panel.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_connector.h>
#include <drm/drm_device.h>
#include <drm/drm_modes.h>

#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/of_graph.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 start*/
#include <linux/of_gpio.h>
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 end*/

/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 start*/
#include <uapi/drm/mi_disp.h>
/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 end*/
/*N17 code for HQ-292463 by p-chenzimo at 2023/05/05 start*/
#include <linux/hqsysfs.h>

/*N17 code for HQ-292463 by p-chenzimo at 2023/05/05 end*/
/*N17 code for HQ-296959 by p-lizongrui at 2023/05/29 start*/

#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"

/*N17 code for HQ-296959 by p-lizongrui at 2023/05/29 end*/
#define CONFIG_MTK_PANEL_EXT
#if defined(CONFIG_MTK_PANEL_EXT)
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"
#endif

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
#include <drm/drm_connector.h>
#include "../mediatek/mediatek_v2/mtk_notifier_odm.h"

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 start*/
#define MAX_BRIGHTNESS_CLONE 16383

/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 end*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static int blank;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
#define REGFLAG_DELAY           0xFFFC
#define REGFLAG_UDELAY          0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW       0xFFFE
#define REGFLAG_RESET_HIGH      0xFFFF

#define FRAME_WIDTH                 1080
#define FRAME_HEIGHT                2400

#define PHYSICAL_WIDTH              69552
#define PHYSICAL_HEIGHT             154960

/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
#define DATA_RATE                   1016
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
#define HSA                         4
#define HBP                         64
#define VSA                         2
#define VBP                         10
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
#define HFP                         151
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
/*Parameter setting for mode 0 Start*/
#define MODE_0_FPS                  60
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
#define MODE_0_VFP                  2452
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
#define MODE_0_HFP                  HFP
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
#define MODE_0_DATA_RATE            1016
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*Parameter setting for mode 0 End*/

/*Parameter setting for mode 1 Start*/
#define MODE_1_FPS                  90
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
#define MODE_1_VFP                  780
#define MODE_1_HFP                  HFP
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
#define MODE_1_DATA_RATE            1016
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*Parameter setting for mode 1 End*/

/*Parameter setting for mode 2 Start*/
#define MODE_2_FPS                  120
#define MODE_2_VFP                  20
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
#define MODE_2_HFP                  HFP
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
#define MODE_2_DATA_RATE            1016
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*Parameter setting for mode 2 End*/

#define LFR_EN                      1
/* DSC RELATED */

#define DSC_ENABLE                  0
#define DSC_VER                     17
#define DSC_SLICE_MODE              1
#define DSC_RGB_SWAP                0
#define DSC_DSC_CFG                 34
#define DSC_RCT_ON                  1
#define DSC_BIT_PER_CHANNEL         8
#define DSC_DSC_LINE_BUF_DEPTH      9
#define DSC_BP_ENABLE               1
#define DSC_BIT_PER_PIXEL           128
//define DSC_PIC_HEIGHT
//define DSC_PIC_WIDTH
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

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
struct drm_notifier_data g_notify_data1;

struct lcm {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vci_en_gpio;
	struct gpio_desc *dvdd_en_gpio;
	bool prepared;
	bool enabled;

/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	int dynamic_fps;
	struct mutex panel_lock;
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 start*/
	bool hbm_enabled;
	const char *panel_info;
/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 end*/
	int error;
/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 start*/
	u32 doze_state;
	u32 doze_brightness_state;
	u32 max_brightness_clone;
/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 end*/
/*N17 code for HQ-299473 by p-chenzimo at 2023/06/15 start*/
	int gir_status;
/*N17 code for HQ-299473 by p-chenzimo at 2023/06/15 end*/
/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 start*/
	enum doze_brightness_state doze_brightness;
};
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
#define PANEL_INFO(fmt, ...)                                                      \
	pr_info("%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 end*/

/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 start*/
static const char *panel_name = "panel_name=dsi_n17_36_02_0a_dsc_vdo";
/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 end*/
static char bl_tb0[] = { 0x51, 0x3f, 0xff };
static u32 bdg_support;
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
static struct drm_panel * g_panel = NULL;
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*N17 code for HQ-296959 by p-lizongrui at 2023/05/29 start*/
static int last_bl_level = 0;
/*N17 code for HQ-296959 by p-lizongrui at 2023/05/29 end*/
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 start*/
static int last_non_zero_bl_level = 0;

#ifdef CONFIG_MI_DISP
extern void mi_dsi_panel_tigger_dimming_work(struct mtk_dsi *dsi);
extern int get_panel_dead_flag(void);
#endif
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 end*/

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
#define lcm_dcs_write_seq(ctx, seq...)                                     \
	({                                                                     \
		const u8 d[] = {seq};                                          \
		BUILD_BUG_ON_MSG(ARRAY_SIZE(d) > 64,                           \
				 "DCS sequence too big for stack");            \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

#define lcm_dcs_write_seq_static(ctx, seq...)                              \
	({                                                                     \
		static const u8 d[] = {seq};                                   \
		lcm_dcs_write(ctx, d, ARRAY_SIZE(d));                      \
	})

/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
static struct LCM_setting_table set_gir_on[] = {
	{0x5F, 01, {0x00}},
	{0x26, 01, {0x03}},
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x00}},
	{0x6F, 01, {0x03}},
	{0xC0, 01, {0x53}},
	{0xF0, 05, {0x55,0xAA,0x52,0x00,0x00}},
};
static struct LCM_setting_table set_gir_off[] = {
	{0x5F, 01, {0x01}},
	{0x26, 01, {0x00}},
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x00}},
	{0x6F, 01, {0x03}},
	{0xC0, 01, {0x20}},
	{0xF0, 05, {0x55,0xAA,0x52,0x00,0x00}},
};
static struct LCM_setting_table dimming_set_enable[] = {
	{0x53, 01, {0x28}},
};
static struct LCM_setting_table dimming_set_disable[] = {
	{0x53, 01, {0x20}},
};
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end*/

static inline struct lcm *panel_to_lcm(struct drm_panel *panel)
{
	return container_of(panel, struct lcm, panel);
}

/* N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start */
static int panel_ddic_send_cmd(struct LCM_setting_table *table,
	unsigned int count, bool block)
{
	int i = 0, j = 0, k = 0;
	int ret = 0;
	unsigned char temp[25][255] = {0};
	unsigned char cmd = {0};
	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 0,
		.flags = MIPI_DSI_MSG_USE_LPM,
		.tx_cmd_num = count,
	};
	if (table == NULL) {
		pr_err("invalid ddic cmd \n");
		return ret;
	}
	if (count == 0 || count > 25) {
		pr_err("cmd count invalid, value:%d \n", count);
		return ret;
	}
	for (i = 0;i < count; i++) {
		memset(temp[i], 0, sizeof(temp[i]));
		/* LCM_setting_table format: {cmd, count, {para_list[]}} */
		cmd = (u8)table[i].cmd;
		temp[i][0] = cmd;
		for (j = 0; j < table[i].count; j++) {
			temp[i][j+1] = table[i].para_list[j];
		}
		cmd_msg.type[i] = table[i].count > 1 ? 0x39 : 0x15;
		cmd_msg.tx_buf[i] = temp[i];
		cmd_msg.tx_len[i] = table[i].count + 1;
		for (k = 0; k < cmd_msg.tx_len[i]; k++) {
			pr_info("%s cmd_msg.tx_buf:0x%02x\n", __func__, temp[i][k]);
		}
		pr_info("%s cmd_msg.tx_len:%d\n", __func__, cmd_msg.tx_len[i]);
	}
	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, block, false);
	if (ret != 0) {
		pr_err("%s: failed to send ddic cmd\n", __func__);
	}
	return ret;
}
/* N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end */

#ifdef PANEL_SUPPORT_READBACK
static int lcm_dcs_read(struct lcm *ctx, u8 cmd, void *data, size_t len)
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
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

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static void lcm_panel_get_data(struct lcm *ctx)
{
	u8 buffer[3] = {0};
	static int ret;

	if (ret == 0) {
		ret = lcm_dcs_read(ctx, 0x0A, buffer, 1);
		dev_info(ctx->dev, "return %d data(0x%08x) to dsi engine\n",
			 ret, buffer[0] | (buffer[1] << 8));
	}
}
static struct LCD_setting_table lcm_suspend_setting[] = {
	{0xFF, 2, {0xFF, 0x10} },
	{0x28, 2, {0x28, 0x00} },

};
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
#endif

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
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
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

#define HFP_SUPPORT 0

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static void lcm_panel_init(struct lcm *ctx)
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
{
/*N17 code for HQ-291618 by p-chenzimo at 2023/05/09 start*/
	usleep_range(1 * 1000, 2 * 1000);
/*N17 code for HQ-291618 by p-chenzimo at 2023/05/09 end*/
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(11 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(5 * 1000);
	gpiod_set_value(ctx->reset_gpio, 1);
	udelay(13 * 1000);
	// optimization
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	mutex_lock(&ctx->panel_lock);
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x80);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x17);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x14);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0x40);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x81);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x13);
	lcm_dcs_write_seq_static(ctx, 0xF9, 0x01);
	// AOD Video Mode
	lcm_dcs_write_seq_static(ctx, 0x17, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x71, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x8D, 0x00, 0x00, 0x04, 0x37, 0x00, 0x00, 0x09, 0x5F);
	udelay(100);
	// set video drop time
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x81);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x3C);
	lcm_dcs_write_seq_static(ctx, 0xF5, 0x81);
	// CMD_P1 ELVDD_LVDET_OFF
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x07);
	// CMD_P0
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0xB9, 0x00, 0x04, 0x38);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0xBD, 0x09, 0x60);
	udelay(100);
	// Column number:1080
	lcm_dcs_write_seq_static(ctx, 0x2A, 0x00, 0x00, 0x04, 0x37);
	udelay(100);
	// Row number:2400
	lcm_dcs_write_seq_static(ctx, 0x2B, 0x00, 0x00, 0x09, 0x5F);
	udelay(100);
	// COMPRESSION_METHOD
	lcm_dcs_write_seq_static(ctx, 0x90, 0x03, 0x43);
	udelay(100);
	// PPS configuration
	lcm_dcs_write_seq_static(ctx, 0x91, 0x89, 0x28, 0x00, 0x14, 0xC2, 0x00, 0x02, 0x0E, 0x01, 0xE8, 0x00, 0x07, 0x05, 0x0E, 0x05, 0x16, 0x10, 0xF0);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x93, 0x89, 0x28, 0x00, 0x14, 0xC2, 0x00, 0x02, 0x0E, 0x01, 0xE8, 0x00, 0x07, 0x05, 0x0E, 0x05, 0x16, 0x10, 0xF0);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x95, 0x89, 0x28, 0x00, 0x14, 0xC2, 0x00, 0x02, 0x0E, 0x01, 0xE8, 0x00, 0x07, 0x05, 0x0E, 0x05, 0x16, 0x10, 0xF0);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x97, 0x89, 0x28, 0x00, 0x14, 0xC2, 0x00, 0x02, 0x0E, 0x01, 0xE8, 0x00, 0x07, 0x05, 0x0E, 0x05, 0x16, 0x10, 0xF0);
	udelay(100);
	//  BCTRL
	lcm_dcs_write_seq_static(ctx, 0x53, 0x20);
	// VBP/VFP Video Mode
/*N17 code for HQ-292295 by p-lizongrui at 2023/05/16 start*/
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x00, 0x0C, 0x00, 0x14);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
/*N17 code for HQ-292295 by p-lizongrui at 2023/05/16 end*/
	udelay(100);
	// TE enable
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	lcm_dcs_write_seq_static(ctx, 0x35, 0x00);
	// DBV
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	udelay(100);
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	if (ctx->dynamic_fps == 120) {
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
		lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
	} else if (ctx->dynamic_fps == 90) {
		lcm_dcs_write_seq_static(ctx, 0x2F, 0x01);
	} else if (ctx->dynamic_fps == 60) {
		lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
	} else {
		lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	}
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	// GIR OFF
/*N17 code for HQ-299473 by p-chenzimo at 2023/06/15 start*/
	if (ctx->gir_status == 1) {
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
		lcm_dcs_write_seq_static(ctx, 0x5F, 0x00);
		lcm_dcs_write_seq_static(ctx, 0x26, 0x03);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		udelay(100);
		lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
		lcm_dcs_write_seq_static(ctx, 0xC0, 0x53);
	} else {
		lcm_dcs_write_seq_static(ctx, 0x5F, 0x01);
		lcm_dcs_write_seq_static(ctx, 0x26, 0x00);
		lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
		udelay(100);
		lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
		lcm_dcs_write_seq_static(ctx, 0xC0, 0x20);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	}
/*N17 code for HQ-299473 by p-chenzimo at 2023/06/15 end*/
	// ESD Config
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0xBE, 0x47, 0x00);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x05);
	lcm_dcs_write_seq_static(ctx, 0xBE, 0x18);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x0F);
	lcm_dcs_write_seq_static(ctx, 0xBE, 0xFB, 0xFB);
	udelay(100);
	// IC may reload failed, force reload gamma again
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0xE8, 0x30);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x84);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x21);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0xFF, 0xFF, 0xF9, 0xFF, 0xFF, 0xFF);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x9C);
	lcm_dcs_write_seq_static(ctx, 0xF4, 0xFF, 0xC0);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	udelay(100);
/*N17 code for HQ-296959 by p-lizongrui at 2023/05/29 start*/
	// Hsync
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x03);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
	lcm_dcs_write_seq_static(ctx, 0xBA, 0x80);
	lcm_dcs_write_seq_static(ctx, 0x6F, 0x2C);
	lcm_dcs_write_seq_static(ctx, 0xC6, 0xFF);
/*N17 code for HQ-296959 by p-lizongrui at 2023/05/29 end*/
/*N17 code for HQ-299837 by p-lizongrui at 2023/07/13 start*/
	lcm_dcs_write_seq_static(ctx, 0xF0,0x55,0xAA,0x52,0x08,0x01);
	lcm_dcs_write_seq_static(ctx, 0x6F,0x05);
	lcm_dcs_write_seq_static(ctx, 0xC7,0x27,0x08);
	lcm_dcs_write_seq_static(ctx, 0xF0,0x55,0xAA,0x52,0x08,0x01);
	lcm_dcs_write_seq_static(ctx, 0x6F,0x09);
	lcm_dcs_write_seq_static(ctx, 0xC7,0x24);
	lcm_dcs_write_seq_static(ctx, 0xF0,0x55,0xAA,0x52,0x08,0x05);
	lcm_dcs_write_seq_static(ctx, 0xCB,0x33,0x33,0x33,0x33,0x33,0x33,0x33);
	lcm_dcs_write_seq_static(ctx, 0xF0,0x55,0xAA,0x52,0x08,0x00);
	lcm_dcs_write_seq_static(ctx, 0xEA,0x80);
/*N17 code for HQ-299837 by p-lizongrui at 2023/07/13 end*/
	lcm_dcs_write_seq_static(ctx, 0x11);
	mdelay(120);
	lcm_dcs_write_seq_static(ctx, 0x29);
	// CMD_P1
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01);
	udelay(100);

	lcm_dcs_write_seq_static(ctx, 0x6F, 0x03);
	// ELVDD_LVDET_ON
	lcm_dcs_write_seq_static(ctx, 0xC7, 0x47);
	// ESD Detection
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0xBE, 0x47, 0xC5);
	udelay(100);
	// Page
	lcm_dcs_write_seq_static(ctx, 0xF0, 0x55, 0xAA, 0x52, 0x00, 0x00);
	udelay(100);
	lcm_dcs_write_seq_static(ctx, 0xFF, 0xAA, 0x55, 0xA5, 0x00);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	udelay(100);
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	mutex_unlock(&ctx->panel_lock);
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static int lcm_disable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!ctx->enabled)
		return 0;

	if (ctx->backlight) {
		ctx->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx->backlight);
	}

	ctx->enabled = false;

	return 0;
}
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 start*/
static int panel_init_power(struct drm_panel *panel);
static int panel_power_down(struct drm_panel *panel);
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 end*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/

static int lcm_unprepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!ctx->prepared)
		return 0;
	pr_info("%s\n", __func__);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	blank = DRM_BLANK_POWERDOWN;
	g_notify_data1.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_POWERDOWN ++++", __func__);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	mutex_lock(&ctx->panel_lock);
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(1);
	lcm_dcs_write_seq_static(ctx, 0x10);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	msleep(120);
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	mutex_unlock(&ctx->panel_lock);
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*N17 code for HQ-291618 by p-chenzimo at 2023/05/09 start*/
	udelay(2000);
/*N17 code for HQ-291618 by p-chenzimo at 2023/05/09 end*/

	ctx->error = 0;
	ctx->prepared = false;
	return 0;
}

/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 start*/
extern atomic_t esd_flag_triggered;
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 end*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static int lcm_prepare(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	int ret;
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 start*/
	char bl_tb_tmp[] = {0x51, 0x00, 0x00};
	bl_tb_tmp[1] = (last_non_zero_bl_level >> 8) & 0xFF;
	bl_tb_tmp[2] = last_non_zero_bl_level & 0xFF;
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 end*/

	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 start*/
	panel_init_power(panel);
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 end*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/

	lcm_panel_init(ctx);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 start*/
	if (atomic_read(&esd_flag_triggered)) {
		mutex_lock(&ctx->panel_lock);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
		lcm_dcs_write(ctx, bl_tb_tmp, ARRAY_SIZE(bl_tb_tmp));
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
		mutex_unlock(&ctx->panel_lock);
		atomic_set(&esd_flag_triggered, 0);
	}
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 end*/

	ret = ctx->error;
	if (ret < 0)
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
		lcm_unprepare(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	ctx->prepared = true;
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_rst(panel);
#endif

#ifdef PANEL_SUPPORT_READBACK
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	lcm_panel_get_data(ctx);
#endif

	blank = DRM_BLANK_UNBLANK;
	g_notify_data1.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_UNBLANK ++++", __func__);

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	return ret;
}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static int lcm_enable(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

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
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
	.clock = (FRAME_WIDTH + MODE_0_HFP + HSA + HBP) *
		(FRAME_HEIGHT + MODE_0_VFP + VSA + VBP) * MODE_0_FPS / 1000, //htotal*vtotal*fps/1000
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_0_HFP,
	.hsync_end = FRAME_WIDTH + MODE_0_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_0_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_0_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_0_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_0_VFP + VSA + VBP,
};

static const struct drm_display_mode performance_mode_1 = {
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
	.clock = (FRAME_WIDTH + MODE_1_HFP + HSA + HBP) *
		(FRAME_HEIGHT + MODE_1_VFP + VSA + VBP) * MODE_1_FPS / 1000,
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_1_HFP,
	.hsync_end = FRAME_WIDTH + MODE_1_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_1_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_1_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_1_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_1_VFP + VSA + VBP,
};

static const struct drm_display_mode performance_mode_2 = {
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
	.clock = (FRAME_WIDTH + MODE_2_HFP + HSA + HBP) *
		(FRAME_HEIGHT + MODE_2_VFP + VSA + VBP) * MODE_2_FPS / 1000,
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)

static struct mtk_panel_params ext_params = {
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
/*N17 code for HQ-305797 by p-chenzimo at 2023/07/06 start*/
//	.vfp_low_power = 0,
/*N17 code for HQ-305797 by p-chenzimo at 2023/07/06 end*/
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
/*N17 code for HQ-298493 by p-chenzimo at 2023/06/25 start*/
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
/*N17 code for HQ-298493 by p-chenzimo at 2023/06/25 end*/
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = 1,
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
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
		.dfps_cmd_table[0] = {60, 2, {0x2F, 0x02} },
		.dfps_cmd_table[1] = {90, 2, {0x2F, 0x01} },
		.dfps_cmd_table[2] = {120, 2, {0x2F, 0x00} },
		.dfps_cmd_table[3] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01} },
		.dfps_cmd_table[4] = {0, 2, {0xEA, 0x91} },
		.dfps_cmd_table[5] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x03} },
		.dfps_cmd_table[6] = {0, 2, {0x6F, 0x03} },
		.dfps_cmd_table[7] = {0, 2, {0xBA, 0x80} },
		.dfps_cmd_table[8] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x00, 0x00} },
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 end*/
	},
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	.dyn = {
		.switch_en = 0,
		.pll_clk = 415,
		.hfp = 400,
		.vfp = 780,
		.vsa = 2,
		.vfp_lp_dyn = 780,
	},
};

static struct mtk_panel_params ext_params_mode_1 = {
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
/*N17 code for HQ-305797 by p-chenzimo at 2023/07/06 start*/
//	.vfp_low_power = 2452,//60hz
/*N17 code for HQ-305797 by p-chenzimo at 2023/07/06 end*/
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 end*/
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = 1,
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
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
		.dfps_cmd_table[0] = {60, 2, {0x2F, 0x02} },
		.dfps_cmd_table[1] = {90, 2, {0x2F, 0x01} },
		.dfps_cmd_table[2] = {120, 2, {0x2F, 0x00} },
		.dfps_cmd_table[3] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01} },
		.dfps_cmd_table[4] = {0, 2, {0xEA, 0x91} },
		.dfps_cmd_table[5] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x03} },
		.dfps_cmd_table[6] = {0, 2, {0x6F, 0x03} },
		.dfps_cmd_table[7] = {0, 2, {0xBA, 0x80} },
		.dfps_cmd_table[8] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x00, 0x00} },
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 end*/
	},
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	.dyn = {
		.switch_en = 0,
		.pll_clk = 415,
		.hfp = 400,
		.vfp = 780,
		.vsa = 2,
		.vfp_lp_dyn = 780,
	},
};

static struct mtk_panel_params ext_params_mode_2 = {
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
/*N17 code for HQ-305797 by p-chenzimo at 2023/07/06 start*/
//	.vfp_low_power = 2452,//60hz
/*N17 code for HQ-305797 by p-chenzimo at 2023/07/06 end*/
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 end*/
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x9c,
	},
	.physical_width_um = PHYSICAL_WIDTH,
	.physical_height_um = PHYSICAL_HEIGHT,
	.dsc_params = {
		.bdg_dsc_enable = 1,
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
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
		.dfps_cmd_table[0] = {60, 2, {0x2F, 0x02} },
		.dfps_cmd_table[1] = {90, 2, {0x2F, 0x01} },
		.dfps_cmd_table[2] = {120, 2, {0x2F, 0x00} },
		.dfps_cmd_table[3] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01} },
		.dfps_cmd_table[4] = {0, 2, {0xEA, 0x91} },
		.dfps_cmd_table[5] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x03} },
		.dfps_cmd_table[6] = {0, 2, {0x6F, 0x03} },
		.dfps_cmd_table[7] = {0, 2, {0xBA, 0x80} },
		.dfps_cmd_table[8] = {0, 6, {0xF0, 0x55, 0xAA, 0x52, 0x00, 0x00} },
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 end*/
	},
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	.dyn = {
		.switch_en = 0,
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
		.pll_clk = 508,
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
		.hfp = 400,
		.vfp = 20,
		.vsa = 2,
		.vfp_lp_dyn = 780,
	},
};

/*N17 code for HQ-296959 by p-lizongrui at 2023/05/29 start*/
static void panel_elvss_control(struct drm_panel *panel, bool en)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}
	if (last_bl_level == 0) {
		pr_info("last bl level is 0, skip set dimming %s\n", __func__, en ? "on" : "off");
		return;
	}
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	if (!ctx->prepared) {
		pr_err("panel unprepared, skip!!!\n");
		return;
	}

/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
	if (en) {
		panel_ddic_send_cmd(dimming_set_enable, ARRAY_SIZE(dimming_set_enable), true);
	} else {
		panel_ddic_send_cmd(dimming_set_disable, ARRAY_SIZE(dimming_set_disable), true);
	}
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end*/
}

/*N17 code for HQ-296959 by p-lizongrui at 2023/05/29 end*/
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 start*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	struct lcm *ctx = panel_to_lcm(g_panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	bl_tb0[1] = level >> 8 & 0x0F;
	bl_tb0[2] = level & 0xFF;
	if (!cb)
		return -1;
	pr_info("%s last_bl_level = %d, backlight = %d=0x%04x 0x%02x 0x%02x\n",__func__,last_bl_level,level,level,bl_tb0[1],bl_tb0[2]);
	mutex_lock(&ctx->panel_lock);
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	mutex_unlock(&ctx->panel_lock);
	if (last_bl_level == 0 && level > 0) {
#ifdef CONFIG_MI_DISP
#ifndef CONFIG_FACTORY_BUILD
		if (!get_panel_dead_flag() && ctx->doze_state == 0)
			mi_dsi_panel_tigger_dimming_work(dsi);
#endif
#endif
	}
	last_bl_level = level;
	if (level != 0)
		last_non_zero_bl_level = level;
	return 0;
}
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 end*/

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
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
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	int ret = 0;
	struct drm_display_mode *m = get_mode_by_id(connector, mode);

	if (m == NULL) {
		pr_info("%s:%d invalid display_mode\n", __func__, __LINE__);
		return -1;
	}
	if (drm_mode_vrefresh(m) == MODE_0_FPS)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m) == MODE_1_FPS)
		ext->params = &ext_params_mode_1;
	else if (drm_mode_vrefresh(m) == MODE_2_FPS)
		ext->params = &ext_params_mode_2;
	else
		ret = 1;
	if (!ret)
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
		ctx->dynamic_fps = drm_mode_vrefresh(m);
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	gpiod_set_value(ctx->reset_gpio, on);

	return 0;
}

/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
static void mode_switch_to_60(struct drm_panel *panel)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx = panel_to_lcm(panel);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x02);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x00, 0x0C, 0x09, 0x94);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	mutex_unlock(&ctx->panel_lock);
}

static void mode_switch_to_90(struct drm_panel *panel)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx = panel_to_lcm(panel);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x01);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x00, 0x0C, 0x03, 0x3A);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	mutex_unlock(&ctx->panel_lock);
}

static void mode_switch_to_120(struct drm_panel *panel)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx = panel_to_lcm(panel);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write_seq_static(ctx, 0x2F, 0x00);
	lcm_dcs_write_seq_static(ctx, 0x3B, 0x00, 0x0C, 0x00, 0x14);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	mutex_unlock(&ctx->panel_lock);
}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	struct drm_display_mode *m = get_mode_by_id(connector, dst_mode);

	if (!m) {
		pr_err("%s:%d invalid display_mode\n", __func__, __LINE__);
		return ret;
	}

	pr_info("%s cur_mode = %d dst_mode %d\n", __func__, cur_mode, dst_mode);

	if (drm_mode_vrefresh(m) == 60) {
		mode_switch_to_60(panel);
	} else if (drm_mode_vrefresh(m) == 90) {
		mode_switch_to_90(panel);
	} else if (drm_mode_vrefresh(m) == 120) {
		mode_switch_to_120(panel);
	} else
		ret = 1;
	if (!ret)
		ctx->dynamic_fps = drm_mode_vrefresh(m);

	return ret;
}

/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
static int panel_ata_check(struct drm_panel *panel)
{
#ifdef BDG_PORTING_DBG
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	unsigned char data[3] = {0x00, 0x00, 0x00};
	unsigned char id[3] = {0x6e, 0x48, 0x00};
	ssize_t ret;

	ret = mipi_dsi_dcs_read(dsi, 0x4, data, 3);
	if (ret < 0) {
		pr_info("%s error\n", __func__);
		return 0;
	}

	pr_info("ATA read data %x %x %x\n", data[0], data[1], data[2]);

	if (data[0] == id[0])
		return 1;

	pr_info("ATA expect read data is %x %x %x\n",
			id[0], id[1], id[2]);
#endif
	return 1;
}

/*N17 code for HQ-291618 by p-chenzimo at 2023/05/09 start*/
static int panel_init_power(struct drm_panel *panel)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 start*/
	if (gpiod_get_value(ctx->vci_en_gpio) && gpiod_get_value(ctx->dvdd_en_gpio)) {
		pr_info("[LCM][%s][%d]panel power had been up\n", __func__, __LINE__);
		return 0;
	}
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 end*/
	gpiod_set_value(ctx->dvdd_en_gpio, 1);
	udelay(2000);
	gpiod_set_value(ctx->vci_en_gpio, 1);
	udelay(7000);
	return 0;
}

static int panel_power_down(struct drm_panel *panel)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 start*/
	if (!gpiod_get_value(ctx->vci_en_gpio) && !gpiod_get_value(ctx->dvdd_en_gpio)) {
		pr_info("[LCM][%s][%d]panel power had been down\n", __func__, __LINE__);
		return 0;
	}
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 end*/
	udelay(2000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(2000);
	gpiod_set_value(ctx->vci_en_gpio, 0);
	udelay(7000);
	gpiod_set_value(ctx->dvdd_en_gpio, 0);
	return 0;
}
/*N17 code for HQ-291618 by p-chenzimo at 2023/05/09 end*/
/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 start*/
static int panel_get_panel_info(struct drm_panel *panel, char *buf)
	{
	int count = 0;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!panel) {
		pr_err(": panel is NULL\n", __func__);
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);

	return count;
}

static int panel_normal_hbm_control(struct drm_panel *panel, uint32_t level)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	mutex_lock(&ctx->panel_lock);
	if (level == 1) {
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
		lcm_dcs_write_seq_static(ctx, 0x51, 0x0F, 0xFF);
	} else if (level == 0) {
		lcm_dcs_write_seq_static(ctx, 0x51, 0x07, 0xFF);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	}
	mutex_unlock(&ctx->panel_lock);

	return 0;
}

static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	if (level > 2047) {
		ctx->hbm_enabled = true;
	}
	pr_err("lcm_setbacklight_control backlight %d\n", level);

	return 0;
}

static bool get_lcm_initialized(struct drm_panel *panel)
{
	bool ret = false;

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	ret = ctx->prepared;
	return ret;
}
/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 end*/

/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 start*/
#ifdef CONFIG_MI_DISP_VDO_TO_CMD_AOD
static unsigned long panel_doze_get_mode_flags(struct drm_panel *panel,
	int doze_en)
{
	unsigned long mode_flags;

	pr_info("%s + \n", __func__);

	if (doze_en) {
		mode_flags = MIPI_DSI_MODE_LPM
		       | MIPI_DSI_MODE_EOT_PACKET
		       | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	} else {
		mode_flags = MIPI_DSI_MODE_VIDEO
		       | MIPI_DSI_MODE_VIDEO_BURST
		       | MIPI_DSI_MODE_LPM | MIPI_DSI_MODE_EOT_PACKET
		       | MIPI_DSI_CLOCK_NON_CONTINUOUS;
	}

	return mode_flags;
}

static struct LCM_setting_table lcm_normal_to_aod[] = {
	/* AOD setting */
	{REGFLAG_CMD, 3, {0xF0,0xAA,0x00} },
	{REGFLAG_CMD, 9, {0xCC,0x00,0x00,0x04,0x37, 0x00,0x00,0x07,0x07} },  //1080*1800
	{REGFLAG_CMD, 3, {0xF0,0xAA,0x12} },
	/* insert 10 black frames */
	{REGFLAG_CMD, 2, {0xD6,0x0A} },
	/* aod1 60nits */
	{REGFLAG_CMD, 2, {0x6D,0x00} },
	/* Sleep in*/
	{REGFLAG_CMD, 1, {0x28} },
	/* pass frmae to demura ram: cmd mode */
	{REGFLAG_CMD, 2, {0x6F,0x02} },
	{REGFLAG_CMD, 1, {0x39} },
	{REGFLAG_DELAY, 10, {} },
	/* Display on */
	{REGFLAG_CMD, 1, {0x29} },
};

static int panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	unsigned int i = 0;

	pr_info("%s + \n", __func__);
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	mutex_lock(&ctx->panel_lock);
	for (i = 0; i < (sizeof(lcm_normal_to_aod) /
			sizeof(struct LCM_setting_table)); i++) {
		unsigned int cmd;

		cmd = lcm_normal_to_aod[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(lcm_normal_to_aod[i].count);
			break;
		case REGFLAG_UDELAY:
			udelay(lcm_normal_to_aod[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			cb(dsi, handle, lcm_normal_to_aod[i].para_list,
				lcm_normal_to_aod[i].count);
		}
	}
	mutex_unlock(&ctx->panel_lock);

	return 0;
}

static int panel_doze_enable_start(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	int cmd = 0x28;
	pr_info("%s + \n", __func__);

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	mutex_lock(&ctx->panel_lock);
	ctx->doze_state = 1;
	cb(dsi, handle, &cmd, 1);
	mutex_unlock(&ctx->panel_lock);

	return 0;
}

static struct LCM_setting_table lcm_aod_to_normal[] = {
	{REGFLAG_CMD, 1, {0x38} },
	{REGFLAG_DELAY, 20, {} },
	/* demura RAM NO access: video mode */
	{REGFLAG_CMD, 2, {0x6F,0x01} },
	/* normal fps setting */
	{REGFLAG_CMD, 2, {0x6C,0x00} },
};

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	unsigned int i = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	mutex_lock(&ctx->panel_lock);
	pr_info("%s: ctx->dynamic_fps = %d\n", __func__, ctx->dynamic_fps);
	if (ctx->dynamic_fps == 120) {
		if (lcm_aod_to_normal[AOD_TO_NORMAL_FPS_INDEX].cmd == 0x6C)
			lcm_aod_to_normal[AOD_TO_NORMAL_FPS_INDEX].para_list[1] = 0x02;
	} else if (ctx->dynamic_fps == 90) {
		if (lcm_aod_to_normal[AOD_TO_NORMAL_FPS_INDEX].cmd == 0x6C)
			lcm_aod_to_normal[AOD_TO_NORMAL_FPS_INDEX].para_list[1] = 0x01;
	} else if (ctx->dynamic_fps == 60) {
		if (lcm_aod_to_normal[AOD_TO_NORMAL_FPS_INDEX].cmd == 0x6C)
			lcm_aod_to_normal[AOD_TO_NORMAL_FPS_INDEX].para_list[1] = 0x00;
	}

	/* Switch back to VDO mode */
	for (i = 0; i < (sizeof(lcm_aod_to_normal) /
			sizeof(struct LCM_setting_table)); i++) {
		unsigned int cmd;

		cmd = lcm_aod_to_normal[i].cmd;

		switch (cmd) {
		case REGFLAG_DELAY:
			msleep(lcm_aod_to_normal[i].count);
			break;
		case REGFLAG_UDELAY:
			udelay(lcm_aod_to_normal[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			break;
		default:
			cb(dsi, handle, lcm_aod_to_normal[i].para_list,
				lcm_aod_to_normal[i].count);
		}
	}
	ctx->doze_state = 0;

	mutex_unlock(&ctx->panel_lock);

	return 0;
}

static int panel_doze_post_disp_on(struct drm_panel *panel,
		void *dsi, dcs_write_gce cb, void *handle)
{
	pr_info("%s + \n", __func__);

/*
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	int cmd = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	cmd = 0x29;
	mutex_lock(&ctx->panel_lock);
	cb(dsi, handle, &cmd, 1);
	mutex_unlock(&ctx->panel_lock);
*/
	return 0;
}

static struct LCM_setting_table lcm_aod_high_mode[] = {
	/* aod 60nit*/
	{REGFLAG_CMD, 2, {0x6D, 0x00} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static struct LCM_setting_table lcm_aod_low_mode[] = {
	/* aod 5nit*/
	{REGFLAG_CMD, 2, {0x6D, 0x02} },
	{REGFLAG_END_OF_TABLE, 0x00, {} }
};

static int panel_set_aod_light_mode(void *dsi,
	dcs_write_gce cb, void *handle, unsigned int mode)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	int i = 0;

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	pr_info("debug for lcm %s\n", __func__);

	mutex_lock(&ctx->panel_lock);
	if (mode == DOZE_BRIGHTNESS_HBM) {
		for (i = 0; i < sizeof(lcm_aod_high_mode)/sizeof(struct LCM_setting_table); i++)
			cb(dsi, handle, lcm_aod_high_mode[i].para_list, lcm_aod_high_mode[i].count);
	} else if (mode == DOZE_BRIGHTNESS_LBM){
		for (i = 0; i < sizeof(lcm_aod_low_mode)/sizeof(struct LCM_setting_table); i++)
			cb(dsi, handle, lcm_aod_low_mode[i].para_list, lcm_aod_low_mode[i].count);
	}
	mutex_unlock(&ctx->panel_lock);
	pr_info("%s : %d !\n", __func__, mode);

	return 0;
}

#ifdef CONFIG_MI_DISP
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
static struct LCM_setting_table lcm_aod_high_mode_setting[] = {
	{0x6D, 01, {0x00}},
};
static struct LCM_setting_table lcm_aod_low_mode_setting[] = {
	{0x6D, 01, {0x02}},
};
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end*/

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness)
{
	int ret = 0;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!panel) {
		pr_err("invalid params\n");
		return -1;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (ctx->doze_brightness_state == doze_brightness) {
		pr_info("%s skip same doze_brightness set:%d\n", __func__, doze_brightness);
		return 0;
	}

/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
	if (DOZE_BRIGHTNESS_LBM  == doze_brightness)
		panel_ddic_send_cmd(lcm_aod_low_mode_setting, ARRAY_SIZE(lcm_aod_low_mode_setting), true);
	else if (DOZE_BRIGHTNESS_HBM == doze_brightness)
		panel_ddic_send_cmd(lcm_aod_high_mode_setting, ARRAY_SIZE(lcm_aod_high_mode_setting), true);
	else
		panel_ddic_send_cmd(lcm_aod_low_mode_setting, ARRAY_SIZE(lcm_aod_low_mode_setting), true);
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end*/

	ctx->doze_brightness_state = doze_brightness;
	pr_info("%s set doze_brightness %d end -\n", __func__, doze_brightness);
	return ret;
}

static int panel_get_doze_brightness(struct drm_panel *panel, u32 *doze_brightness)
{
	int count = 0;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	*doze_brightness = ctx->doze_brightness_state;
	pr_info("%s get doze_brightness %d end -\n", __func__, *doze_brightness);
	return count;
}
#endif
#else
#ifdef CONFIG_MI_DISP
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
static struct LCM_setting_table LCM_AOD_HBM[] = {
	{0x51, 02, {0x00,0xF5}},
};
static struct LCM_setting_table LCM_AOD_LBM[] = {
	{0x51, 02, {0x00,0x14}},
};
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end*/

/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 start*/
static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness) {
	struct lcm *ctx = NULL;
	int ret = 0;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = panel_to_lcm(panel);
	/*
		DOZE_TO_NORMAL = 0,
		DOZE_BRIGHTNESS_HBM = 1,
		DOZE_BRIGHTNESS_LBM = 2,
	*/
	PANEL_INFO("doze_brightness = %d\n", doze_brightness);
	switch (doze_brightness) {
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
		case DOZE_BRIGHTNESS_HBM:
			panel_ddic_send_cmd(LCM_AOD_HBM, ARRAY_SIZE(LCM_AOD_HBM), true);
			break;
		case DOZE_BRIGHTNESS_LBM:
			panel_ddic_send_cmd(LCM_AOD_LBM, ARRAY_SIZE(LCM_AOD_LBM), true);
			break;
		case DOZE_TO_NORMAL:
			return ret;
			break;
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end*/
		default:
			pr_err("%s: doze_brightness is invalid\n", __func__);
			ret = -1;
			goto err;
	}
	ctx->doze_brightness = doze_brightness;
err:
/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 end*/
	return ret;
}

/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 start*/
int panel_get_doze_brightness(struct drm_panel *panel, u32 *brightness) {
	struct lcm *ctx = NULL;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return -1;
	}
	ctx = panel_to_lcm(panel);
	*brightness = ctx->doze_brightness;
	return 0;
/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 end*/
}
#endif
#endif
static int panel_get_max_brightness_clone(struct drm_panel *panel, u32 *max_brightness_clone)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	*max_brightness_clone = ctx->max_brightness_clone;

	return 0;
}

/*N17 code for HQ-291688 by p-chenzimo at 2023/06/06 start*/
static int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	struct device_node *chosen;
	char *tmp_buf = NULL;
	int tmp_size = 0;
	chosen = of_find_node_by_path("/chosen");
	if (chosen) {
		tmp_buf = (char *)of_get_property(chosen, "lcm_white_point", (int *)&tmp_size);
		if (tmp_size > 0) {
			strncpy(buf, tmp_buf, tmp_size);
			pr_info("[%s]: white_point = %s, size = %d\n", __func__, buf, tmp_size);
		}
	} else {
		pr_info("[%s]:find chosen failed\n", __func__);
	}

	return tmp_size;
}

/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 start*/
extern int mtkfb_set_backlight_level(unsigned int level);
/*N17 code for HQ-291688 by p-chenzimo at 2023/06/06 end*/
#ifdef CONFIG_MI_DISP_ESD_CHECK
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static void lcm_esd_restore_backlight(struct mtk_dsi *dsi, struct drm_panel *panel)
{
/*	char bl_tb0[] = {0x51, 0x00, 0x00};
	struct lcm *ctx;

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}

	ctx = panel_to_lcm(panel);

	bl_tb0[1] = (last_non_zero_bl_level >> 8) & 0xFF;
	bl_tb0[2] = last_non_zero_bl_level & 0xFF;

	pr_info("%s: restore to level = %d\n", __func__, last_non_zero_bl_level);

	mutex_lock(&ctx->panel_lock);
	lcm_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
	mutex_unlock(&ctx->panel_lock);
*/
#ifdef CONFIG_MI_DISP
#ifndef CONFIG_FACTORY_BUILD
	mi_dsi_panel_tigger_dimming_work(dsi);
#endif
#endif

	return;
}
#endif
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

/*N17 code for HQ-299473 by p-chenzimo at 2023/06/15 start*/
static int panel_set_gir_on(struct drm_panel *panel)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	int ret = 0;

	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	ctx->gir_status = 1;
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
		panel_ddic_send_cmd(set_gir_on, ARRAY_SIZE(set_gir_on), true);
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end*/
	}

err:
	return ret;
}

static int panel_set_gir_off(struct drm_panel *panel)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	int ret = -1;

	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	ctx->gir_status = 0;
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
		panel_ddic_send_cmd(set_gir_off, ARRAY_SIZE(set_gir_off), true);
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end*/
	}

err:
	return ret;
}

static int panel_get_gir_status(struct drm_panel *panel)
{
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	if (!panel) {
		pr_err("%s; panel is NULL\n", __func__);
		return -1;
	}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = panel_to_lcm(panel);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	return ctx->gir_status;
}

/*N17 code for HQ-299473 by p-chenzimo at 2023/06/15 end*/
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 end*/
/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 end*/
static struct mtk_panel_funcs ext_funcs = {
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	.mode_switch = mode_switch,
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
	.ata_check = panel_ata_check,
/*N17 code for HQ-291618 by p-chenzimo at 2023/05/09 start*/
	.init_power = panel_init_power,
	.power_down = panel_power_down,
/*N17 code for HQ-291618 by p-chenzimo at 2023/05/09 end*/
/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 start*/
	.get_panel_info = panel_get_panel_info,
	.normal_hbm_control = panel_normal_hbm_control,
	.setbacklight_control = lcm_setbacklight_control,
	.get_panel_initialized = get_lcm_initialized,
/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 end*/
/*N17 code for HQ-296959 by p-lizongrui at 2023/05/29 start*/
	.panel_elvss_control = panel_elvss_control,
/*N17 code for HQ-296959 by p-lizongrui at 2023/05/29 end*/
/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 start*/
#ifdef CONFIG_MI_DISP_VDO_TO_CMD_AOD
	/* add for ramless AOD */
	.doze_get_mode_flags = panel_doze_get_mode_flags,
	.doze_enable = panel_doze_enable,
	.doze_enable_start = panel_doze_enable_start,
	//.doze_area = panel_doze_area,
	.doze_disable = panel_doze_disable,
	.doze_post_disp_on = panel_doze_post_disp_on,
	.set_aod_light_mode = panel_set_aod_light_mode,
#endif
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
	.get_panel_max_brightness_clone = panel_get_max_brightness_clone,
/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 end*/
/*N17 code for HQ-291688 by p-chenzimo at 2023/06/06 start*/
	.get_wp_info = panel_get_wp_info,
/*N17 code for HQ-291688 by p-chenzimo at 2023/06/06 end*/
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 start*/
#ifdef CONFIG_MI_DISP_ESD_CHECK
	.esd_restore_backlight = lcm_esd_restore_backlight,
#endif
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 end*/
/*N17 code for HQ-299473 by p-chenzimo at 2023/06/15 start*/
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
/*N17 code for HQ-299473 by p-chenzimo at 2023/06/15 end*/
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
	 *           become ready and start receiving video data
	 * @enable: the time (in milliseconds) that it takes for the panel to
	 *          display the first valid frame after starting to receive
	 *          video data
	 * @disable: the time (in milliseconds) that it takes for the panel to
	 *           turn the display off (no content is visible)
	 * @unprepare: the time (in milliseconds) that it takes for the panel
	 *             to power itself down completely
	 */
	struct {
		unsigned int prepare;
		unsigned int enable;
		unsigned int disable;
		unsigned int unprepare;
	} delay;
};

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static int lcm_get_modes(struct drm_panel *panel,
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
			struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	struct drm_display_mode *mode_1;
	struct drm_display_mode *mode_2;

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

	mode_1 = drm_mode_duplicate(connector->dev, &performance_mode_1);
	if (!mode_1) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_1.hdisplay,
			performance_mode_1.vdisplay,
			drm_mode_vrefresh(&performance_mode_1));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_1);
	mode_1->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_1);


	mode_2 = drm_mode_duplicate(connector->dev, &performance_mode_2);
	if (!mode_2) {
		dev_info(connector->dev->dev, "failed to add mode %ux%ux@%u\n",
			performance_mode_2.hdisplay,
			performance_mode_2.vdisplay,
			drm_mode_vrefresh(&performance_mode_2));
		return -ENOMEM;
	}
	drm_mode_set_name(mode_2);
	mode_2->type = DRM_MODE_TYPE_DRIVER;
	drm_mode_probed_add(connector, mode_2);

	connector->display_info.width_mm = 64;
	connector->display_info.height_mm = 129;

	return 3;
}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = lcm_disable,
	.unprepare = lcm_unprepare,
	.prepare = lcm_prepare,
	.enable = lcm_enable,
	.get_modes = lcm_get_modes,
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
};

static void check_is_bdg_support(struct device *dev)
{
	unsigned int ret = 0;

	ret = of_property_read_u32(dev->of_node, "bdg-support", &bdg_support);
	if (!ret && bdg_support == 1) {
		pr_info("%s, bdg support 1", __func__);
	} else {
		pr_info("%s, bdg support 0", __func__);
		bdg_support = 0;
	}
}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
extern int panel_event;
static ssize_t panel_event_show(struct device *device,
			struct device_attribute *attr,
			char *buf)
{
	ssize_t ret = 0;
	struct drm_connector *connector = dev_get_drvdata(device);
	if (!connector) {
		pr_info("%s-%d connector is NULL \r\n",__func__, __LINE__);
		return ret;
	}
	return snprintf(buf, PAGE_SIZE, "%d\n", panel_event);
}

static DEVICE_ATTR_RO(panel_event);

static struct attribute *panel1_attrs[] = {
	&dev_attr_panel_event.attr,
        NULL,
};

static const struct attribute_group panel1_attr_group = {
        .attrs = panel1_attrs,
};

static int lcm_probe(struct mipi_dsi_device *dsi)
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	struct lcm *ctx;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	int ret;

	pr_info("%s+ n17-36-02-0a-dsc-vdo,lcm\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("n17-36-02-0a-dsc-vdo,lcm %s\n", remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info(" n17-36-02-0a-dsc-vdo,lcm isn't current lcm\n");
		return -ENODEV;
	}
	pr_info("%s+\n", __func__);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ctx = devm_kzalloc(dev, sizeof(struct lcm), GFP_KERNEL);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_info(dev, "cannot get reset-gpios %ld\n",
			PTR_ERR(ctx->reset_gpio));
		return PTR_ERR(ctx->reset_gpio);
	}

	ctx->vci_en_gpio = devm_gpiod_get(dev, "vci-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->vci_en_gpio)) {
		dev_info(dev, "cannot get vci_en_gpios %ld\n",
			PTR_ERR(ctx->vci_en_gpio));
		return PTR_ERR(ctx->vci_en_gpio);
	}

	ctx->dvdd_en_gpio = devm_gpiod_get(dev, "dvdd-en", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->dvdd_en_gpio)) {
		dev_info(dev, "cannot get dvdd_en_gpios %ld\n",
			PTR_ERR(ctx->dvdd_en_gpio));
		return PTR_ERR(ctx->dvdd_en_gpio);
	}

	ctx->prepared = true;
	ctx->enabled = true;

/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 start*/
	ctx->panel_info = panel_name;
/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 end*/
/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 start*/
	ctx->max_brightness_clone = MAX_BRIGHTNESS_CLONE;
/*N17 code for HQ-296776 by p-chenzimo at 2023/06/01 end*/
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	ctx->dynamic_fps = 60;

/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 start*/
	ext_params.err_flag_irq_gpio = of_get_named_gpio_flags(
		dev->of_node, "mi,esd-err-irq-gpio",
		0, (enum of_gpio_flags *)&(ext_params.err_flag_irq_flags));
	ext_params_mode_1.err_flag_irq_gpio = ext_params_mode_1.err_flag_irq_gpio;
	ext_params_mode_1.err_flag_irq_flags = ext_params_mode_1.err_flag_irq_flags;
	ext_params_mode_2.err_flag_irq_gpio = ext_params_mode_2.err_flag_irq_gpio;
	ext_params_mode_2.err_flag_irq_flags = ext_params_mode_2.err_flag_irq_flags;
/*N17 code for HQ-290979 by p-chenzimo at 2023/06/13 end*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	drm_panel_init(&ctx->panel, dev, &lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	check_is_bdg_support(dev);
#if defined(CONFIG_MTK_PANEL_EXT)
	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;
#endif
/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 start*/
	ctx->hbm_enabled = false;
/*N17 code for HQ-291715 by p-chenzimo at 2023/05/18 end*/
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 start*/
	g_panel = &ctx->panel;
/*N17 code for HQ-290795 by p-chenzimo at 2023/04/26 end*/
/*N17 code for HQ-292463 by p-chenzimo at 2023/05/05 start*/
	hq_regiser_hw_info(HWID_LCM, "oncell,vendor:36,IC:02");
/*N17 code for HQ-292463 by p-chenzimo at 2023/05/05 end*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
	ret = sysfs_create_group(&dev->kobj, &panel1_attr_group);
	if (ret)
		return ret;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	pr_info("%s-\n", __func__);

	return ret;
}

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static int lcm_remove(struct mipi_dsi_device *dsi)
{
	struct lcm *ctx = mipi_dsi_get_drvdata(dsi);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
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

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
static const struct of_device_id lcm_of_match[] = {
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	{
		.compatible = "n17-36-02-0a-dsc-vdo,lcm",
	},
	{} };

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	.driver = {
		.name = "panel-n17-36-02-0a-dsc-vdo",
		.owner = THIS_MODULE,
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
		.of_match_table = lcm_of_match,
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/
	},
};

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 start*/
module_mipi_dsi_driver(lcm_driver);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/14 end*/

MODULE_AUTHOR("Jingjing Liu <jingjing.liu@mediatek.com>");
MODULE_DESCRIPTION("tianma nt36672E vdo 120hz 6382 Panel Driver");
MODULE_LICENSE("GPL v2");

