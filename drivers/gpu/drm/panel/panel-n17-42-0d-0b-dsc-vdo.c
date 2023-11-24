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
/*N17 code for HQ-299504 by p-lizongrui at 2023/06/26 start*/
#include <linux/of_gpio.h>
/*N17 code for HQ-299504 by p-lizongrui at 2023/06/26 end*/
#include <linux/hqsysfs.h>
/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 start*/
#include <uapi/drm/mi_disp.h>
/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 end*/
#include "../mediatek/mediatek_v2/mtk_panel_ext.h"
#include "../mediatek/mediatek_v2/mtk_drm_graphics_base.h"

/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 start*/
#include "../mediatek/mediatek_v2/mi_disp/mi_panel_ext.h"

/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 end*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 start*/
#include <drm/drm_connector.h>
#include "../mediatek/mediatek_v2/mtk_notifier_odm.h"

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 end*/
#define REGFLAG_DELAY 0xFFFC
#define REGFLAG_UDELAY 0xFFFB
#define REGFLAG_END_OF_TABLE 0xFFFD
#define REGFLAG_RESET_LOW 0xFFFE
#define REGFLAG_RESET_HIGH 0xFFFF

#define FRAME_WIDTH 1080
#define FRAME_HEIGHT 2400

#define PHYSICAL_WIDTH 69552
#define PHYSICAL_HEIGHT 154960

#define DATA_RATE 1016
#define HSA 8
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
#define HBP 16
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
#define VSA 4
#define VBP 8
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
#define HFP 194

#define MODE_0_FPS 60
#define MODE_0_VFP 2452
#define MODE_0_HFP HFP
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
#define MODE_0_DATA_RATE DATA_RATE

#define MODE_1_FPS 90
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
#define MODE_1_VFP 828
#define MODE_1_HFP HFP
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
#define MODE_1_DATA_RATE DATA_RATE

#define MODE_2_FPS 120
#define MODE_2_VFP 20
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
#define MODE_2_HFP HFP
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
#define MODE_2_DATA_RATE DATA_RATE

#define LFR_EN 1
/* DSC RELATED */

#define DSC_ENABLE 0
#define DSC_VER 17
#define DSC_SLICE_MODE 1
#define DSC_RGB_SWAP 0
#define DSC_DSC_CFG 34
#define DSC_RCT_ON 1
#define DSC_BIT_PER_CHANNEL 8
#define DSC_DSC_LINE_BUF_DEPTH 9
#define DSC_BP_ENABLE 1
#define DSC_BIT_PER_PIXEL 128
#define DSC_SLICE_HEIGHT 12
#define DSC_SLICE_WIDTH 540
#define DSC_CHUNK_SIZE 540
#define DSC_XMIT_DELAY 512
#define DSC_DEC_DELAY 526
#define DSC_SCALE_VALUE 32
#define DSC_INCREMENT_INTERVAL 287
#define DSC_DECREMENT_INTERVAL 7
#define DSC_LINE_BPG_OFFSET 12
#define DSC_NFL_BPG_OFFSET 2235
#define DSC_SLICE_BPG_OFFSET 2170
#define DSC_INITIAL_OFFSET 6144
#define DSC_FINAL_OFFSET 4336
#define DSC_FLATNESS_MINQP 3
#define DSC_FLATNESS_MAXQP 12
#define DSC_RC_MODEL_SIZE 8192
#define DSC_RC_EDGE_FACTOR 6
#define DSC_RC_QUANT_INCR_LIMIT0 11
#define DSC_RC_QUANT_INCR_LIMIT1 11
#define DSC_RC_TGT_OFFSET_HI 3
#define DSC_RC_TGT_OFFSET_LO 3
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 start*/
static int blank;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 end*/
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 start*/
struct drm_notifier_data g_notify_data1;

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 end*/
struct panel {
	struct device *dev;
	struct drm_panel panel;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *vci_en_gpio;
	struct gpio_desc *dvdd_en_gpio;
	bool prepared;
	bool enabled;
	int dynamic_fps;
	struct mutex panel_lock;
	const char *panel_info;
	int error;
/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 start*/
	int gir_status;
/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 end*/
/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 start*/
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
	enum doze_brightness_state doze_brightness;
};
#define PANEL_INFO(fmt, ...)                                                      \
	pr_info("%s:%d %s: " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 end*/

static const char panel_name[] = "panel_name=dsi_n17_42_0d_0b_dsc_vdo";
static char bl_tb0[] = { 0x51, 0x0f, 0xff };
static struct drm_panel *g_panel = NULL;
/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 start*/
/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 end*/
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
#define panel_6382_dcs_write(ctx, seq...)                                      \
	({                                                                     \
		static const u8 d[] = { seq };                                 \
		_panel_6382_dcs_write(ctx, d, ARRAY_SIZE(d));                  \
	})
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/

/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
static struct LCM_setting_table LCM_AOD_HBM[] = {
	{0x51, 02, {0x00,0xF5}},
};
static struct LCM_setting_table LCM_AOD_LBM[] = {
	{0x51, 02, {0x00,0x14}},
};
static struct LCM_setting_table set_gir_on[] = {
	{0xF0, 02, {0xAA, 0x11}},
	{0xD5, 01, {0x73}},
};
static struct LCM_setting_table set_gir_off[] = {
	{0xF0, 02, {0xAA, 0x11}},
	{0xD5, 01, {0x70}},
};

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

static void _panel_6382_dcs_write(struct panel *ctx, const void *data,
				  size_t len)
{
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx->dev);
	ssize_t ret;
	u8 *addr;
	u8 count = 1;
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
	static u8 last_addr = 0;
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
	if (ctx->error < 0)
		return;

	addr = (u8 *)data;
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
	if (last_addr == 0x35 || last_addr == 0x44)
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
		count++;
	while (count) {
		if (*addr < 0xB0)
			ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
		else
			ret = mipi_dsi_generic_write(dsi, data, len);
		if (ret < 0) {
			dev_info(ctx->dev, "error %zd writing seq: %ph\n", ret,
				 data);
			ctx->error = ret;
		}
		if (len > 2)
			udelay(100);
		count--;
	}
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
	last_addr = *addr;
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
}

static inline struct panel *drm_panel_to_panel(struct drm_panel *panel)
{
	return container_of(panel, struct panel, panel);
}

/*N17 code for HQ-299504 by p-lizongrui at 2023/06/26 start*/
extern atomic_t esd_flag_triggered;
/*N17 code for HQ-299504 by p-lizongrui at 2023/06/26 end*/
static void panel_init(struct panel *ctx)
{
	mutex_lock(&ctx->panel_lock);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(100);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(1);
	gpiod_set_value(ctx->reset_gpio, 0);
	mdelay(2);
	gpiod_set_value(ctx->reset_gpio, 1);
	mdelay(17);

	panel_6382_dcs_write(ctx, 0x03, 0x01);
	panel_6382_dcs_write(ctx, 0x53, 0x20);
	panel_6382_dcs_write(ctx, 0x6F, 0x01);
	panel_6382_dcs_write(ctx, 0x35, 0x00);
	panel_6382_dcs_write(ctx, 0x55, 0x00);
	panel_6382_dcs_write(ctx, 0x59, 0x09);
	if (ctx->dynamic_fps == 120)
		panel_6382_dcs_write(ctx, 0x6C, 0x02);
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
	else if (ctx->dynamic_fps == 90)
		panel_6382_dcs_write(ctx, 0x6C, 0x01);
	else
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
		panel_6382_dcs_write(ctx, 0x6C, 0x00);
	panel_6382_dcs_write(ctx, 0x6D, 0x00);
	panel_6382_dcs_write(ctx, 0x70, 0x11, 0x00, 0x00, 0x89, 0x30, 0x80,
			     0x09, 0x60, 0x04, 0x38, 0x00, 0x0c, 0x02, 0x1c,
			     0x02, 0x1c, 0x02, 0x00, 0x02, 0x0e, 0x00, 0x20,
			     0x01, 0x1f, 0x00, 0x07, 0x00, 0x0c, 0x08, 0xbb,
			     0x08, 0x7a, 0x18, 0x00, 0x10, 0xf0, 0x03, 0x0c,
			     0x20, 0x00, 0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c,
			     0x2a, 0x38, 0x46, 0x54, 0x62, 0x69, 0x70, 0x77,
			     0x79, 0x7b, 0x7d, 0x7e, 0x01, 0x02, 0x01, 0x00,
			     0x09, 0x40, 0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa,
			     0x19, 0xf8, 0x1a, 0x38, 0x1a, 0x78, 0x1a, 0xb6,
			     0x2a, 0xb6, 0x2a, 0xf4, 0x2a, 0xf4, 0x4b, 0x34,
			     0x63, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	panel_6382_dcs_write(ctx, 0xF0, 0xAA, 0x10);
	/*N17 code for HQ-305264 by p-chenzimo at 2023/06/28 start*/
	panel_6382_dcs_write(ctx, 0xD0, 0x84, 0x15);
	/*N17 code for HQ-305264 by p-chenzimo at 2023/06/28 end*/
	panel_6382_dcs_write(ctx, 0x65, 0x04);
	panel_6382_dcs_write(ctx, 0xCF, 0x7F);
	panel_6382_dcs_write(ctx, 0xF0, 0xAA, 0x13);
	panel_6382_dcs_write(ctx, 0xC8, 0x04);
	panel_6382_dcs_write(ctx, 0xD0, 0x04);
	panel_6382_dcs_write(ctx, 0xE0, 0x04);
	panel_6382_dcs_write(ctx, 0xFF, 0x5A, 0x80);
	panel_6382_dcs_write(ctx, 0x65, 0x0E);
	panel_6382_dcs_write(ctx, 0xF9, 0Xb9);
	panel_6382_dcs_write(ctx, 0x65, 0x0a);
	panel_6382_dcs_write(ctx, 0xF9, 0xa8);
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
	// fix hsync error
	panel_6382_dcs_write(ctx, 0xF0, 0xAA, 0x10);
	panel_6382_dcs_write(ctx, 0xB1, 0x01, 0x72, 0x00, 0x0C, 0x09, 0xC2,
			     0x00, 0x01, 0x72, 0x00, 0x0C, 0x03, 0x6A, 0x00,
			     0x01, 0x72, 0x00, 0x0C, 0x00, 0x42, 0x00);
	// panel_6382_dcs_write(ctx, 0xF0, 0xAA, 0x10);
	// panel_6382_dcs_write(ctx, 0xB9, 0x01, 0x34);
	panel_6382_dcs_write(ctx, 0xF0, 0xAA, 0x10);
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
	panel_6382_dcs_write(ctx, 0xB8, 0x90, 0x28, 0x03, 0x00, 0x00, 0x00, 0x00, 0x28, 0x28);
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 end*/
/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 start*/
	if (ctx->gir_status == 1) {
		panel_6382_dcs_write(ctx, 0xF0, 0xAA, 0x11);
		panel_6382_dcs_write(ctx, 0xD5, 0x73);
	} else {
		panel_6382_dcs_write(ctx, 0xF0, 0xAA, 0x11);
		panel_6382_dcs_write(ctx, 0xD5, 0x70);
	}
/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 end*/
	// round on
	panel_6382_dcs_write(ctx, 0xF0, 0xAA, 0x18);
	panel_6382_dcs_write(ctx, 0xB0, 0x13);
	panel_6382_dcs_write(ctx, 0xB2, 0x13);

/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
/*N17 code for HQ-302129 by p-lizongrui at 2023/07/08 start*/
	// 2nit fix
	panel_6382_dcs_write(ctx, 0xF0,0xAA,0x13);
	//90Hz
	panel_6382_dcs_write(ctx, 0xD3,0x0C,0xD6,0x0B,0xE4,0x0B,0x84,0x0A,0xF4,0x09,0xCC,0x07,0x9C,0x04,0x7C);
	//120Hz
	panel_6382_dcs_write(ctx, 0xE3,0x09,0xAE,0x08,0xF8,0x08,0xB0,0x08,0x3C,0x07,0x64,0x05,0xBC,0x03,0x60);

/*N17 code for HQ-302129 by p-lizongrui at 2023/07/08 end*/
	panel_6382_dcs_write(ctx, 0xFF, 0x5A, 0x81);
	panel_6382_dcs_write(ctx, 0x65, 0x08);
	panel_6382_dcs_write(ctx, 0xF6, 0x51, 0x44);
	panel_6382_dcs_write(ctx, 0x65, 0x03);
	panel_6382_dcs_write(ctx, 0xF4, 0x03);
	panel_6382_dcs_write(ctx, 0xFF, 0x5A, 0x00);
	panel_6382_dcs_write(ctx, 0xF0, 0xAA, 0x00);
/*N17 code for HQ-299504 by p-lizongrui at 2023/06/26 start*/
	// esd restore
	if (atomic_read(&esd_flag_triggered)) {
		_panel_6382_dcs_write(ctx, bl_tb0, ARRAY_SIZE(bl_tb0));
		atomic_set(&esd_flag_triggered, 0);
	}
/*N17 code for HQ-299504 by p-lizongrui at 2023/06/26 end*/
	panel_6382_dcs_write(ctx, 0x11);
	mdelay(120);
	panel_6382_dcs_write(ctx, 0x29);
	mutex_unlock(&ctx->panel_lock);
}

static int panel_disable(struct drm_panel *panel)
{
	struct panel *ctx = drm_panel_to_panel(panel);
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
static int panel_unprepare(struct drm_panel *panel)
{
	struct panel *ctx = drm_panel_to_panel(panel);
	if (!ctx->prepared)
		return 0;
	pr_info("%s\n", __func__);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 start*/
	blank = DRM_BLANK_POWERDOWN;
	g_notify_data1.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_POWERDOWN ++++", __func__);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 end*/
	mutex_lock(&ctx->panel_lock);
	panel_6382_dcs_write(ctx, 0x28);
	msleep(1);
	panel_6382_dcs_write(ctx, 0x10);
	msleep(120);
	mutex_unlock(&ctx->panel_lock);
	ctx->error = 0;
	ctx->prepared = false;
	return 0;
}

static int panel_prepare(struct drm_panel *panel)
{
	struct panel *ctx = drm_panel_to_panel(panel);
	int ret;
	pr_info("%s\n", __func__);
	if (ctx->prepared)
		return 0;
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 start*/
	panel_init_power(panel);
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 end*/
	panel_init(ctx);
	ret = ctx->error;
	if (ret < 0)
		panel_unprepare(panel);
	ctx->prepared = true;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 start*/
	blank = DRM_BLANK_UNBLANK;
	g_notify_data1.data = &blank;
	drm_notifier_call_chain(DRM_EVENT_BLANK, &g_notify_data1);
	pr_err("[XMFP] : %s ++++ blank = DRM_BLANK_UNBLANK ++++", __func__);
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 end*/
	return ret;
}

static int panel_enable(struct drm_panel *panel)
{
	struct panel *ctx = drm_panel_to_panel(panel);
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
	.clock = (FRAME_WIDTH + MODE_0_HFP + HSA + HBP) *
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
		 (FRAME_HEIGHT + MODE_0_VFP + VSA + VBP) * MODE_0_FPS / 1000,
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
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
	.clock = (FRAME_WIDTH + MODE_1_HFP + HSA + HBP) *
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
		 (FRAME_HEIGHT + MODE_1_VFP + VSA + VBP) * MODE_1_FPS / 1000,
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
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
	.clock = (FRAME_WIDTH + MODE_2_HFP + HSA + HBP) *
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
		 (FRAME_HEIGHT + MODE_2_VFP + VSA + VBP) * MODE_2_FPS / 1000,
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/
	.hdisplay = FRAME_WIDTH,
	.hsync_start = FRAME_WIDTH + MODE_2_HFP,
	.hsync_end = FRAME_WIDTH + MODE_2_HFP + HSA,
	.htotal = FRAME_WIDTH + MODE_2_HFP + HSA + HBP,
	.vdisplay = FRAME_HEIGHT,
	.vsync_start = FRAME_HEIGHT + MODE_2_VFP,
	.vsync_end = FRAME_HEIGHT + MODE_2_VFP + VSA,
	.vtotal = FRAME_HEIGHT + MODE_2_VFP + VSA + VBP,
};

static struct mtk_panel_params ext_params = {
/*N17 code for HQ-305797 by p-chenzimo at 2023/07/06 start*/
//	.vfp_low_power = 0,
/*N17 code for HQ-305797 by p-chenzimo at 2023/07/06 end*/
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
	.data_rate = MODE_0_DATA_RATE,
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 60,
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
		.dfps_cmd_table[0] = {60, 2, {0x6c, 0x00}},
		.dfps_cmd_table[1] = {90, 2, {0x6c, 0x01}},
		.dfps_cmd_table[2] = {120, 2, {0x6c, 0x02}},
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 end*/
	}
};

static struct mtk_panel_params ext_params_mode_1 = {
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
/*N17 code for HQ-305797 by p-chenzimo at 2023/07/06 start*/
//	.vfp_low_power = 2452,
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
	.data_rate = MODE_1_DATA_RATE,
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 90,
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
		.dfps_cmd_table[0] = {60, 2, {0x6c, 0x00}},
		.dfps_cmd_table[1] = {90, 2, {0x6c, 0x01}},
		.dfps_cmd_table[2] = {120, 2, {0x6c, 0x02}},
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 end*/
	}
};

static struct mtk_panel_params ext_params_mode_2 = {
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
/*N17 code for HQ-305797 by p-chenzimo at 2023/07/06 start*/
//	.vfp_low_power = 2452,
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
	.data_rate = MODE_2_DATA_RATE,
	.bdg_ssc_enable = 0,
	.ssc_enable = 0,
	.dyn_fps = {
		.switch_en = 1,
		.vact_timing_fps = 120,
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 start*/
		.dfps_cmd_table[0] = {60, 2, {0x6c, 0x00}},
		.dfps_cmd_table[1] = {90, 2, {0x6c, 0x01}},
		.dfps_cmd_table[2] = {120, 2, {0x6c, 0x02}},
/*N17 code for HQ-291716 by p-chenzimo at 2023/06/30 end*/
	}
};

static int panel_setbacklight_cmdq(void *dsi, dcs_write_gce cb, void *handle,
				   unsigned int level)
{
	struct panel *ctx = drm_panel_to_panel(g_panel);
	bl_tb0[1] = level >> 8 & 0x0F;
	bl_tb0[2] = level & 0xFF;
	if (!cb)
		return -1;
	pr_info("%s backlight = %d=0x%04x 0x%02x 0x%02x\n", __func__, level,
		level, bl_tb0[1], bl_tb0[2]);
	mutex_lock(&ctx->panel_lock);
	cb(dsi, handle, bl_tb0, ARRAY_SIZE(bl_tb0));
	mutex_unlock(&ctx->panel_lock);
	return 0;
}

static struct drm_display_mode *get_mode_by_id(struct drm_connector *connector,
					       unsigned int mode)
{
	struct drm_display_mode *m;
	unsigned int i = 0;

	list_for_each_entry (m, &connector->modes, head) {
		if (i == mode)
			return m;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
				   struct drm_connector *connector,
				   unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	struct panel *ctx = drm_panel_to_panel(panel);
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
		ctx->dynamic_fps = drm_mode_vrefresh(m);
	return ret;
}

static int panel_ext_reset(struct drm_panel *panel, int on)
{
	struct panel *ctx = drm_panel_to_panel(panel);
	gpiod_set_value(ctx->reset_gpio, on);
	return 0;
}

static int panel_init_power(struct drm_panel *panel)
{
	struct panel *ctx = drm_panel_to_panel(panel);
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 start*/
	if (gpiod_get_value(ctx->vci_en_gpio) && gpiod_get_value(ctx->dvdd_en_gpio)) {
		pr_info("[LCM][%s][%d]panel power had been up\n", __func__, __LINE__);
		return 0;
	}
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 end*/
	gpiod_set_value(ctx->vci_en_gpio, 1);
	mdelay(2);
	gpiod_set_value(ctx->dvdd_en_gpio, 1);
	mdelay(11);
	return 0;
}

static int panel_power_down(struct drm_panel *panel)
{
	struct panel *ctx = drm_panel_to_panel(panel);
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 start*/
	if (!gpiod_get_value(ctx->vci_en_gpio) && !gpiod_get_value(ctx->dvdd_en_gpio)) {
		pr_info("[LCM][%s][%d]panel power had been down\n", __func__, __LINE__);
		return 0;
	}
/*N17 code for HQ-299519 by p-lizongrui at 2023/07/10 end*/
	udelay(2000);
	gpiod_set_value(ctx->reset_gpio, 0);
	udelay(2000);
	gpiod_set_value(ctx->dvdd_en_gpio, 0);
	udelay(2000);
	gpiod_set_value(ctx->vci_en_gpio, 0);
	return 0;
}

static int panel_get_panel_info(struct drm_panel *panel, char *buf)
{
	int count = 0;
	struct panel *ctx;
	if (!panel) {
		pr_err(": panel is NULL\n", __func__);
		return -EAGAIN;
	}
	ctx = drm_panel_to_panel(panel);
	count = snprintf(buf, PAGE_SIZE, "%s\n", ctx->panel_info);
	return count;
}

static int panel_normal_hbm_control(struct drm_panel *panel, uint32_t level)
{
	struct panel *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = drm_panel_to_panel(panel);
	// TODO
	return 0;
}

static int lcm_setbacklight_control(struct drm_panel *panel, unsigned int level)
{
	return 0;
}

static bool get_lcm_initialized(struct drm_panel *panel)
{
	bool ret = false;
	struct panel *ctx;
	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}
	ctx = drm_panel_to_panel(panel);
	ret = ctx->prepared;
	return ret;
}

/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 start*/
static int panel_set_gir_on(struct drm_panel *panel)
{
	struct panel *ctx = NULL;
	int ret = 0;

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}

	ctx = drm_panel_to_panel(panel);
	pr_info("%s: + ctx->gir_status = %d  \n", __func__, ctx->gir_status);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
		panel_ddic_send_cmd(set_gir_on, ARRAY_SIZE(set_gir_on), true);
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end*/
		ctx->gir_status = 1;
	}

err:
	return ret;
}

static int panel_set_gir_off(struct drm_panel *panel)
{
	struct panel *ctx = NULL;
	int ret = -1;

	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		goto err;
	}

	ctx = drm_panel_to_panel(panel);
	pr_info("%s: + ctx->gir_status = %d \n", __func__, ctx->gir_status);
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
	} else {
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 start*/
		panel_ddic_send_cmd(set_gir_off, ARRAY_SIZE(set_gir_off), true);
/*N17 code for HQ-312810 by p-luozhibin1 at 2023/08/18 end*/
		ctx->gir_status = 0;
	}

err:
	return ret;
}

static int panel_get_gir_status(struct drm_panel *panel)
{
	struct panel *ctx;

	if (!panel) {
		pr_err("%s; panel is NULL\n", __func__);
		return -1;
	}

	ctx = drm_panel_to_panel(panel);

	return ctx->gir_status;
}

/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 end*/
/*N17 code for HQ-299568 by p-chenzimo at 2023/06/20 start*/
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

/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 start*/

static int panel_set_doze_brightness(struct drm_panel *panel, int doze_brightness) {
	struct panel *ctx = NULL;
	int ret = 0;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		ret = -1;
		goto err;
	}
	ctx = drm_panel_to_panel(panel);
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
	return ret;
}

int panel_get_doze_brightness(struct drm_panel *panel, u32 *brightness) {
	struct panel *ctx = NULL;
	if (!panel) {
		pr_err("%s: panel is NULL\n", __func__);
		return -1;
	}
	ctx = drm_panel_to_panel(panel);
	*brightness = ctx->doze_brightness;
	return 0;
}

/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 end*/
/*N17 code for HQ-299568 by p-chenzimo at 2023/06/20 end*/
static struct mtk_panel_funcs ext_funcs = {
	.set_backlight_cmdq = panel_setbacklight_cmdq,
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.init_power = panel_init_power,
	.power_down = panel_power_down,
	.get_panel_info = panel_get_panel_info,
	.normal_hbm_control = panel_normal_hbm_control,
	.setbacklight_control = lcm_setbacklight_control,
	.get_panel_initialized = get_lcm_initialized,
/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 start*/
	.panel_set_gir_on = panel_set_gir_on,
	.panel_set_gir_off = panel_set_gir_off,
	.panel_get_gir_status = panel_get_gir_status,
/*N17 code for HQ-299567 by p-chenzimo at 2023/06/26 end*/
/*N17 code for HQ-299568 by p-chenzimo at 2023/06/20 start*/
	.get_wp_info = panel_get_wp_info,
/*N17 code for HQ-299568 by p-chenzimo at 2023/06/20 end*/
/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 start*/
	.set_doze_brightness = panel_set_doze_brightness,
	.get_doze_brightness = panel_get_doze_brightness,
/*N17 code for HQ-306105 by p-lizongrui at 2023/07/25 end*/
};

static int panel_get_modes(struct drm_panel *panel,
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

static const struct drm_panel_funcs panel_drm_funcs = {
	.disable = panel_disable,
	.unprepare = panel_unprepare,
	.prepare = panel_prepare,
	.enable = panel_enable,
	.get_modes = panel_get_modes,
};

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 start*/
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

static struct attribute *panel2_attrs[] = {
	&dev_attr_panel_event.attr,
        NULL,
};
static const struct attribute_group panel2_attr_group = {
        .attrs = panel2_attrs,
};

/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 end*/
static int panel_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *dsi_node, *remote_node = NULL, *endpoint = NULL;
	struct panel *ctx;
	int ret;

	pr_info("%s+ n17-42-0d-0b-dsc-vdo,lcm\n", __func__);

	dsi_node = of_get_parent(dev->of_node);
	if (dsi_node) {
		endpoint = of_graph_get_next_endpoint(dsi_node, NULL);
		if (endpoint) {
			remote_node = of_graph_get_remote_port_parent(endpoint);
			if (!remote_node) {
				pr_info("No panel connected,skip probe lcm\n");
				return -ENODEV;
			}
			pr_info("n17-42-0d-0b-dsc-vdo,lcm %s\n",
				remote_node->name);
		}
	}
	if (remote_node != dev->of_node) {
		pr_info(" n17-42-0d-0b-dsc-vdo,lcm isn't current lcm\n");
		return -ENODEV;
	}
	pr_info("%s+\n", __func__);
	ctx = devm_kzalloc(dev, sizeof(struct panel), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);

	ctx->dev = dev;
	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE;

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

	ctx->panel_info = panel_name;
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 start*/
	ctx->dynamic_fps = 60;
/*N17 code for HQ-298851 by p-lizongrui at 2023/06/08 end*/

/*N17 code for HQ-299504 by p-lizongrui at 2023/06/26 start*/
	ext_params.err_flag_irq_gpio = of_get_named_gpio_flags(
		dsi_node, "mi,esd-err-irq-gpio",
		0, (enum of_gpio_flags *)&(ext_params.err_flag_irq_flags));
	ext_params_mode_1.err_flag_irq_gpio = ext_params_mode_1.err_flag_irq_gpio;
	ext_params_mode_1.err_flag_irq_flags = ext_params_mode_1.err_flag_irq_flags;
	ext_params_mode_2.err_flag_irq_gpio = ext_params_mode_2.err_flag_irq_gpio;
	ext_params_mode_2.err_flag_irq_flags = ext_params_mode_2.err_flag_irq_flags;

/*N17 code for HQ-299504 by p-lizongrui at 2023/06/26 end*/
	drm_panel_init(&ctx->panel, dev, &panel_drm_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0)
		drm_panel_remove(&ctx->panel);

	mtk_panel_tch_handle_reg(&ctx->panel);
	ret = mtk_panel_ext_create(dev, &ext_params, &ext_funcs, &ctx->panel);
	if (ret < 0)
		return ret;

	g_panel = &ctx->panel;
	hq_regiser_hw_info(HWID_LCM, "oncell,vendor:42,IC:0d");
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 start*/
	ret = sysfs_create_group(&dev->kobj, &panel2_attr_group);
	if (ret)
		return ret;
/*N17 code for HQ-301563 by p-chenzimo at 2023/07/06 end*/
	pr_info("%s-\n", __func__);

	return ret;
}

static int panel_remove(struct mipi_dsi_device *dsi)
{
	struct panel *ctx = mipi_dsi_get_drvdata(dsi);
	struct mtk_panel_ctx *ext_ctx = find_panel_ctx(&ctx->panel);
	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);
	mtk_panel_detach(ext_ctx);
	mtk_panel_remove(ext_ctx);
	return 0;
}

static const struct of_device_id panel_of_match[] = {
	{
		.compatible = "n17-42-0d-0b-dsc-vdo,lcm",
	},
	{}
};

MODULE_DEVICE_TABLE(of, panel_of_match);

static struct mipi_dsi_driver panel_driver = {
	.probe = panel_probe,
	.remove = panel_remove,
	.driver = {
		.name = "panel-n17-42-0d-0b-dsc-vdo",
		.owner = THIS_MODULE,
		.of_match_table = panel_of_match,
	},
};

module_mipi_dsi_driver(panel_driver);

MODULE_AUTHOR("LiZongRui <LiZongRui@huaqin.com>");
MODULE_DESCRIPTION("panel vdo 120hz 6382 Panel Driver");
/*N17 code for HQ-299568 by p-chenzimo at 2023/06/20 start*/
MODULE_LICENSE("GPL v2");
/*N17 code for HQ-299568 by p-chenzimo at 2023/06/20 end*/
