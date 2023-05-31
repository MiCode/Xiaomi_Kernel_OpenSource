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
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_l12a_l.h"
#include "../mediatek/mediatek_v2/mtk_corner_pattern/mtk_data_hw_roundedpattern_l12a_r.h"
#endif
#include <asm/atomic.h>
#include "include/mi_disp_nvt_alpha_data.h"

#define DATA_RATE0		550
#define DATA_RATE1		1100
#define DATA_RATE2		720

#define MODE2_FPS		90
#define MODE2_HFP		160
#define MODE2_HSA		2
#define MODE2_HBP		4
#define MODE2_VFP		54
#define MODE2_VSA		10
#define MODE2_VBP		10

#define MODE1_FPS		120
#define MODE1_HFP		160
#define MODE1_HSA		2
#define MODE1_HBP		4
#define MODE1_VFP		54
#define MODE1_VSA		10
#define MODE1_VBP		10

#define MODE0_FPS		60
#define MODE0_HFP		408
#define MODE0_HSA		2
#define MODE0_HBP		4
#define MODE0_VFP		54
#define MODE0_VSA		10
#define MODE0_VBP		10
#define HACT_WEIGHT		1220
#define VACT_HEIGHT		2712

#define MAX_BRIGHTNESS_CLONE 	16383
#define FPS_INIT_INDEX		11

static char bl_tb0[] = {0x51, 0x3, 0xff};
static const char *panel_name = "panel_name=dsi_l12a_36_02_0b_dsc_cmd";
static atomic_t doze_enable = ATOMIC_INIT(0);
static int gDcThreshold = 450;
static bool gDcEnable = false;
static char oled_grayscale_cmdline[100] = {0};

/*
  6x->l,12->12,61->a,02->second panel
  0x6c126102->l12a second panel,tianma
*/
#define DSC_PANEL_NAME_L12A_TM 0x6c126102


//localhbm white gamma
static char oled_wp_cmdline[16] = {0};
static char oled_lhbm_cmdline[80] = {0};
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
	TYPE_WHITE_900 = 0,
	TYPE_WHITE_110,
	TYPE_GREEN_500,
	TYPE_LHBM_OFF,
	TYPE_HLPM_W900,
	TYPE_HLPM_W110,
	TYPE_HLPM_G500,
	TYPE_HLPM_OFF,
	TYPE_MAX
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


//AMOLED 1.25V:DVDD_1P6,GPIO147
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

static struct LCM_setting_table init_setting[] = {
	//Power Off Regulator Timing =7
	{0xFF, 04,  {0xAA,0x55,0xA5,0x80}},
	{0x6F, 01,  {0x0A}},
	{0xF6, 03,  {0x70,0x70,0x70}},
	{0x6F, 01,  {0x0E}},
	{0xF6, 01,  {0x70}},
	//with no black&AOD sequence
	{0x6F, 01,  {0x2D}},
	{0xFC, 01,  {0x44}},
	{0xF0, 05,  {0x55,0xAA,0x52,0x08,0x01}},
	{0x6F, 01,  {0x01}},
	{0xD2, 04,  {0x00,0x00,0x00,0x20}},
	{0xE4, 03,  {0x80,0x05,0x5C}},
	{0x6F, 01,  {0x07}},
	{0xE4, 02,  {0x05,0x5C}},
	{0x6F, 01,  {0x0A}}, 
	{0xE4, 04,  {0x80,0x05,0x5C}},
	{0x6F, 01,  {0x21}},
	{0xD8, 03,  {0x60,0x60,0x60}},
	//VGL_REG shift 1/2 Frame
	{0x6F, 01,  {0x1E}},
	{0xD8, 01,  {0x24}},
	//PVDD&PVEE OFF Shift 1 Frame
	{0xCD, 01,  {0x61}},
	//GOA power on/off Timing
	{0xF0, 05,  {0x55,0xAA,0x52,0x08,0x05}},
	{0xB3, 02,  {0x84,0x85}},
	{0xB5, 02,  {0x84,0x85}},
	{0xB7, 04,  {0x84, 0x00,0x00,0x84}},
	{0xB8, 04,  {0x84, 0x00,0x00,0x84}},
	//Power Off Frame NO.=6
	{0xF0, 05,  {0x55,0xAA,0x52,0x08,0x00}},
	{0x6F, 01,  {0x07}},
	{0xCA, 03,  {0x08,0x06,0x58}},
	//OD common
	{0xF0, 05,  {0x55,0xAA,0x52,0x08,0x08}},
	{0xE0, 05,  {0x41,0x73,0x73,0x73,0x83}},
	{0xE1, 21,  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xE2, 21,  {0x80,0xEF,0x66,0x86,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x01,0x00}},
	{0xE3, 21,  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xE4, 18,  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xE5, 18,  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xE6, 18,  {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0xF0, 05,  {0x55,0xAA,0x52,0x08,0x07}},
	{0x6F, 01,  {0x37}},
	{0xB7, 03,  {0x08,0x08,0x08}},
	{0xF0, 05,  {0x55,0xAA,0x52,0x08,0x00}},
	{0x6F, 01,  {0x37}},
	{0xB7, 03,  {0x08,0x08,0x08}},
	{0xF0, 05,  {0x55,0xAA,0x52,0x08,0x00}},
	{0x6F, 01,  {0x01}},
	{0xC0, 01,  {0x20}},
	{0xF0, 05,  {0x55,0xAA,0x52,0x08,0x08}},
	{0xEA, 33,  {0x02,0x00,0xE0,0x00,0xE0,0x70,0x00,0x70,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}},
	{0x6F, 01,  {0x15}},
	{0xEA, 06,  {0x00,0x00,0x80,0x00,0x80,0x00}},
	{0xF0, 05,  {0x55,0xAA,0x52,0x08,0x08}},
	{0x6F, 01,  {0x21}},
	{0xEA, 06,  {0x00,0x00,0x00,0x00,0x00,0x00}},
	//60hz
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xE0, 01, {0x01}},
	{0xE2, 04, {0x88,0xE0,0xCC,0x0C}},
	{0x6F, 01, {0x12}},
	{0xE2, 02, {0x86,0x01}},
	{0xEA,  8, {0x02,0x00,0xE0,0x00,0xE0,0x70,0x00,0x70}},
	{0x6F, 01, {0x17}},
	{0xEA, 03, {0x80,0x00,0x80}},


	//DSOP OFF
	{0xF0,	05,  {0x55,0xAA,0x52,0x08,0x00}},
	{0xC6,	16,  {0xAD,0xAA,0xAA,0xA3,0xAD,0xAA,0xAA,0xA3,0xAD,0xAA,0xAA,0xA3,0xAD,0xAA,0xAA,0xA3}},
	//dimming setting
	{0xF0,	05,  {0x55,0xAA,0x52,0x08,0x00}},
	{0xB2,	01,  {0x09}},
	{0x6F,	01,  {0x05}},
	{0xB2,	02,  {0x08,0x08}},
	{0xFF,	04,  {0xAA,0x55,0xA5,0x81}},
	{0x6F,	01,  {0x19}},
	{0xFB,	01,  {0x00}},
	{0x6F,	01,  {0x05}},
	{0xFE,	01,  {0x34}},
	{0xFF,	04,  {0xAA,0x55,0xA5,0x80}},
	{0x6F,	01,  {0x1A}},
	{0xF4,	01,  {0x55}},
	{0x6F,	01,  {0x0F}},
	{0xfc,	01,  {0x00}},
	{0x6F,	01,  {0x09}},
	{0xFc,	02,  {0xFC,0xF0}},
	{0xFF,	04,  {0xAA,0x55,0xA5,0x83}},
	{0x6F,	01,  {0x12}},
	{0xFE,	01,  {0x41}},
	{0x5F,	01,  {0x01}},	//GIR OFF
	{0x26,	01,  {0x00}},
	{0x17,	01,  {0x10}},
	{0x2A,	04,  {0x00,0x00,0x04,0xC3}},
	{0x2B,	04,  {0x00,0x00,0x0A,0x97}},
	{0x2F,	01,  {0x00}},	//60.5Hz
	{0x35,	01,  {0x30}},
	//90hz te wight
	{0x44,	02,  {0x00,0x00}},
	{0x51,	02,  {0x00,0x00}},
	{0x6F,	01,  {0x04}},
	{0x51,	02,  {0x03,0xFF}},
	{0x53,	01,  {0x20}},
	{0x90,	01,  {0x03}},
	{0x91,	18,  {0xAB, 0x28, 0x00, 0x0C, 0xD2, 0x00, 0x02, 0x4C, 0x01, 0x24, 0x00, 0x08, 0x09, 0x75, 0x07, 0x7B, 0x10, 0xF0}},
	{0xFF,	04,  {0xAA,0x55,0xA5,0x81}},
	{0x6F,	01,  {0x0B}},
	{0xFD,	01,  {0x80}},
	{0xFF,	04,  {0xAA,0x55,0xA5,0x81}},
	{0x6F,	01,  {0x0E}},
	{0xF5,	01,  {0x00}},
	//OSC=127.9525M
	{0xFF,	04,  {0xAA,0x55,0xA5,0x80}},
	{0x6F,	01,  {0x15}},
	{0xF8,	02,  {0x01,0x68}},
	{0x6F,	01,  {0x31}},
	{0xF8,	02,  {0x01,0x0E}},
	{0x6F,	01,  {0x01}},
	{0x1F,	01,  {0x06}},

	//esd active high
	{0xF0,	05,  {0x55,0xAA,0x52,0x08,0x00}},
	{0x6F,	01,  {0x01}},
	{0xBE,	01,  {0x45}},
	{0x6F,	01,  {0x05}},
	{0xBE,	01,  {0x88}},
	//EM reset by frame
	{0xF0,  05,  {0x55,0xAA,0x52,0x08,0x00}},
	{0x6F,  01,  {0x13}},
	{0xB2,  01,  {0x4F}},
	//TM AOD Flicker
	{0xF0,	05,  {0x55,0xAA,0x52,0x08,0x00}},
	{0xE2,	01,  {0x40}},
	{0x6F,	01,  {0x57}},
	{0xE2,	05,  {0xFF,0xFF,0xFF,0xFF,0xFF}},
	{0x6F,	01,  {0x37}},
	{0xE2,	04,  {0x80,0x08,0x08,0x0C}},

	//TM Dotted line
	{0xF0,	05,  {0x55,0xAA,0x52,0x08,0x01}},
	{0xDD,	01,  {0x40}},

	{0x11,	01,  {0x00}},
	{REGFLAG_DELAY, 120, {}},
	{0x29,	01,  {0x00}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lhbm_normal_white_900nit[] = {
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 02, {0x6F, 0x31}},
	{0xDF, 02, {0xDF, 0x22}},
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xD0, 07, {0xD0, 0x09, 0x30, 0x08, 0x1c, 0x0A, 0x30}},
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 02, {0x6F, 0x4C}},
	{0xDF, 03, {0xDF, 0x1F, 0xFC}},
	{0x88, 06, {0x88, 0x01, 0x02, 0x62, 0x09,0x8C}},
	{0x87, 04, {0x87, 0x25, 0x0F, 0xFF}},
};

static struct LCM_setting_table lhbm_hlpm_white_900nit[] = {
	//aod off
	{0x65, 02, {0x65, 0x00}},
	{0x38, 02, {0x38, 0x00}},
	{0x51, 07, {0x51, 0x00, 0xF6, 0x00, 0xF6, 0x03, 0xFF}},
	{0x2c, 02, {0x2c, 0x00}},
	//normal lhbm setting
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 02, {0x6F, 0x31}},
	{0xDF, 02, {0xDF, 0x22}},
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xD0, 07, {0xD0, 0x09, 0x30, 0x08, 0x1c, 0x0A, 0x30}},
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 02, {0x6F, 0x4C}},
	{0xDF, 03, {0xDF, 0x1F, 0xFC}},
	{0x88, 06, {0x88, 0x01, 0x02, 0x62, 0x09,0x8C}},
	{0x87, 04, {0x87, 0x25, 0x0F, 0xFF}},
};

static struct LCM_setting_table lhbm_normal_white_110nit[] = {
	//normal lhbm setting
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 02, {0x6F, 0x31}},
	{0xDF, 02, {0xDF, 0x22}},
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xD0, 07, {0xD0, 0x06, 0x55, 0x05, 0xCC, 0x07, 0x1F}},
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 02, {0x6F, 0x4C}},
	{0xDF, 03, {0xDF, 0x05, 0x20}},
	{0x88, 06, {0x88, 0x01, 0x02, 0x62, 0x09,0x8C}},
	{0x87, 04, {0x87, 0x25, 0x06, 0xA0}},
};

static struct LCM_setting_table lhbm_hlpm_white_110nit[] = {
	//aod off
	{0x65, 02, {0x65, 0x00}},
	{0x38, 02, {0x38, 0x00}},
	{0x51, 07, {0x51, 0x00, 0x14, 0x00, 0x14, 0x01, 0xFF}},
	{0x2c, 02, {0x2c, 0x00}},
	//normal lhbm setting
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 02, {0x6F, 0x31}},
	{0xDF, 02, {0xDF, 0x22}},
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xD0, 07, {0xD0, 0x06, 0x55, 0x05, 0xCC, 0x07, 0x1F}},
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 02, {0x6F, 0x4C}},
	{0xDF, 03, {0xDF, 0x05, 0x20}},
	{0x88, 06, {0x88, 0x01, 0x02, 0x62, 0x09,0x8C}},
	{0x87, 04, {0x87, 0x25, 0x06, 0xA0}},
};


static struct LCM_setting_table lhbm_normal_green_500nit[] = {
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 02, {0x6F, 0x31}},
	{0xDF, 02, {0xDF, 0x22}},
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x02}},
	{0xD0, 07, {0xD0, 0x00, 0x00, 0x07, 0xBC, 0x00, 0x00}},
	{0xF0, 06, {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x6F, 02, {0x6F, 0x4C}},
	{0xDF, 03, {0xDF, 0x1F, 0xFC}},
	{0x88, 06, {0x88, 0x01, 0x02, 0x62, 0x09,0x8C}},
	{0x87, 04, {0x87, 0x25, 0x0F, 0xFF}},
};

static struct LCM_setting_table lhbm_off[] = {
	{0x51, 03, {0x51, 0x05, 0xff}},
	{0x87, 02, {0x87, 0x00}},
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

	if (!ctx->prepared)
		return 0;

	lcm_dcs_write_seq_static(ctx, 0x28);
	msleep(15);
	lcm_dcs_write_seq_static(ctx, 0x10);
	msleep(125);

	ctx->error = 0;
	ctx->prepared = false;
	ctx->crc_level = 0;
	ctx->doze_suspend = false;

	return 0;
}


static void lcm_panel_init(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s: +\n", __func__);
	if (ctx->prepared) {
		pr_info("%s: panel has been prepared, nothing to do!\n", __func__);
		goto err;
	}

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
	usleep_range(20 * 1000, 20 * 1000 + 10);
	devm_gpiod_put(ctx->dev, ctx->reset_gpio);

	push_table(ctx, init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table));

	ctx->prepared = true;
err:
	pr_info("%s: -\n", __func__);
}

static int lcm_panel_poweroff(struct drm_panel *panel)
{
	struct lcm *ctx = panel_to_lcm(panel);

	if (ctx->prepared)
		return 0;

	pr_info("%s !+\n", __func__);
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

	lcm_panel_vci_disable(ctx->dev);
	udelay(2000);

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
	lcm_panel_vddi_disable(ctx->dev);
	udelay(2000);
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

	//lcm_panel_init(ctx);

	ret = ctx->error;
	if (ret < 0) {
		lcm_unprepare(panel);
		lcm_panel_poweroff(panel);
	}

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

static const struct drm_display_mode default_mode = {
	.clock = 273139,
	.hdisplay = HACT_WEIGHT,
	.hsync_start = HACT_WEIGHT + MODE0_HFP,
	.hsync_end = HACT_WEIGHT + MODE0_HFP + MODE0_HSA,
	.htotal = HACT_WEIGHT + MODE0_HFP + MODE0_HSA + MODE0_HBP,
	.vdisplay = VACT_HEIGHT,
	.vsync_start = VACT_HEIGHT + MODE0_VFP,
	.vsync_end = VACT_HEIGHT + MODE0_VFP + MODE0_VSA,
	.vtotal = VACT_HEIGHT + MODE0_VFP + MODE0_VSA + MODE0_VBP,
};

static const struct drm_display_mode middle_mode = {
	.clock = 347526,
	.hdisplay = HACT_WEIGHT,
	.hsync_start = HACT_WEIGHT + MODE2_HFP,
	.hsync_end = HACT_WEIGHT + MODE2_HFP + MODE2_HSA,
	.htotal = HACT_WEIGHT + MODE2_HFP + MODE2_HSA + MODE2_HBP,
	.vdisplay = VACT_HEIGHT,
	.vsync_start = VACT_HEIGHT + MODE2_VFP,
	.vsync_end = VACT_HEIGHT + MODE2_VFP + MODE2_VSA,
	.vtotal = VACT_HEIGHT + MODE2_VFP + MODE2_VSA + MODE2_VBP,
};

static const struct drm_display_mode performence_mode = {
	.clock = 463368,
	.hdisplay = HACT_WEIGHT,
	.hsync_start = HACT_WEIGHT + MODE1_HFP,
	.hsync_end = HACT_WEIGHT + MODE1_HFP + MODE1_HSA,
	.htotal = HACT_WEIGHT + MODE1_HFP + MODE1_HSA + MODE1_HBP,
	.vdisplay = VACT_HEIGHT,
	.vsync_start = VACT_HEIGHT + MODE1_VFP,
	.vsync_end = VACT_HEIGHT + MODE1_VFP + MODE1_VSA,
	.vtotal = VACT_HEIGHT + MODE1_VFP + MODE1_VSA + MODE1_VBP,
};

#if defined(CONFIG_MTK_PANEL_EXT)
static struct mtk_panel_params ext_params = {
	.pll_clk = DATA_RATE0 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2712,
		.pic_width = 1220,
		.slice_height = 12,
		.slice_width = 610,
		.chunk_size = 610,
		.xmit_delay = 512,
		.dec_delay = 588,
		.scale_value = 32,
		.increment_interval = 292,
		.decrement_interval = 8,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 2421,
		.slice_bpg_offset = 1915,
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
		.dsc_config_panel_name = DSC_PANEL_NAME_L12A_TM,
		},
	.data_rate = DATA_RATE0,
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
	.pll_clk = DATA_RATE2 / 2,
	.cust_esd_check = 0,
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2712,
		.pic_width = 1220,
		.slice_height = 12,
		.slice_width = 610,
		.chunk_size = 610,
		.xmit_delay = 512,
		.dec_delay = 588,
		.scale_value = 32,
		.increment_interval = 292,
		.decrement_interval = 8,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 2421,
		.slice_bpg_offset = 1915,
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
		.dsc_config_panel_name = DSC_PANEL_NAME_L12A_TM,
		},
	.data_rate = DATA_RATE2,
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
	.esd_check_enable = 0,
	.lcm_esd_check_table[0] = {
		.cmd = 0x0a,
		.count = 1,
		.para_list[0] = 0x1c,
	},
	.lcm_color_mode = MTK_DRM_COLOR_MODE_DISPLAY_P3,
	.physical_width_um = 69540,
	.physical_height_um = 154584,
	.output_mode = MTK_PANEL_DSC_SINGLE_PORT,
	.dsc_params = {
		.enable = 1,
		.ver = 17,
		.slice_mode = 1,
		.rgb_swap = 0,
		.dsc_cfg = 40,
		.rct_on = 1,
		.bit_per_channel = 10,
		.dsc_line_buf_depth = 11,
		.bp_enable = 1,
		.bit_per_pixel = 128,
		.pic_height = 2712,
		.pic_width = 1220,
		.slice_height = 12,
		.slice_width = 610,
		.chunk_size = 610,
		.xmit_delay = 512,
		.dec_delay = 588,
		.scale_value = 32,
		.increment_interval = 292,
		.decrement_interval = 8,
		.line_bpg_offset = 13,
		.nfl_bpg_offset = 2421,
		.slice_bpg_offset = 1915,
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
		.dsc_config_panel_name = DSC_PANEL_NAME_L12A_TM,
		},
	.data_rate = DATA_RATE1,
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
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);

	if (drm_mode_vrefresh(m_dst) == MODE0_FPS)
		*ext_param = &ext_params;
	else if (drm_mode_vrefresh(m_dst) == MODE2_FPS)
		*ext_param = &ext_params_90hz;
	else if (drm_mode_vrefresh(m_dst) == MODE1_FPS)
		*ext_param = &ext_params_120hz;
	else
		ret = 1;

	return ret;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int mode)
{
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;
	struct drm_display_mode *m_dst = get_mode_by_id(connector, mode);

	if (drm_mode_vrefresh(m_dst) == MODE0_FPS)
		ext->params = &ext_params;
	else if (drm_mode_vrefresh(m_dst) == MODE2_FPS)
		ext->params = &ext_params_90hz;
	else if (drm_mode_vrefresh(m_dst) == MODE1_FPS)
		ext->params = &ext_params_120hz;
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

static void lcm_onoff_dimming(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char dimmingon_tb[] = {0x53, 0x28};
	char dimmingoff_tb[] = {0x53,0x20};
	char tb0[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00};
	char tb1[] = {0x6F, 0x05};
	char tb2[] = {0xB2, 0x08, 0x08};
	static unsigned int last_bl_level = 0;
	static unsigned int dimming_on_frame = 1;
	if (!level) {
		last_bl_level = level;
		dimming_on_frame = 1;
		return;
	}

	if (8 != dimming_on_frame && (level <= 200 || level > 800)) {
		cb(dsi, handle, dimmingon_tb, ARRAY_SIZE(dimmingon_tb));
		cb(dsi, handle, tb0, ARRAY_SIZE(tb0));
		cb(dsi, handle, tb1, ARRAY_SIZE(tb1));
		cb(dsi, handle, tb2, ARRAY_SIZE(tb2));
		dimming_on_frame = 8;
	} else if (0 != dimming_on_frame && (level > 200 && level < 450)) {
		cb(dsi, handle, dimmingoff_tb, ARRAY_SIZE(dimmingoff_tb));
		dimming_on_frame = 0;
	} else if (4 != dimming_on_frame && (level > 450 && level <= 800)) {
		tb2[1] = tb2[2] = 0x04;
		cb(dsi, handle, dimmingon_tb, ARRAY_SIZE(dimmingon_tb));
		cb(dsi, handle, tb0, ARRAY_SIZE(tb0));
		cb(dsi, handle, tb1, ARRAY_SIZE(tb1));
		cb(dsi, handle, tb2, ARRAY_SIZE(tb2));
		dimming_on_frame = 4;
	}

	last_bl_level = level;
}

bool is_hbm_fod_on(struct mtk_dsi *dsi);

static int lcm_setbacklight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	char bl_tb[] = {0x51, 0x07, 0xff};
	char lhbm_off_page[] = {0x87, 0x00};

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

	if (gDcEnable && level < gDcThreshold && ctx->crc_level)
		level = gDcThreshold;

	bl_tb[1] = (level >> 8) & 0xFF;
	bl_tb[2] = level & 0xFF;

	if (!cb)
		return -1;
	if (atomic_read(&doze_enable)) {
		pr_info("%s: Return it when aod on, %d %d %d!\n", __func__, level, bl_tb[1], bl_tb[2]);
		return 0;
	}

	if (!mtk_dsi->mi_cfg.last_bl_level && level && is_hbm_fod_on(mtk_dsi)) {
		pr_info("lhbm off when first screen on \n");
		cb(dsi, handle, lhbm_off_page, ARRAY_SIZE(lhbm_off_page));
	}

	mtk_dsi->mi_cfg.last_bl_level = level;
	lcm_onoff_dimming(dsi, cb, handle, level);

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

static struct LCM_setting_table mode_120hz_setting_gir_off[] = {
	/* frequency select 120hz */
	{0x2F, 01, {0x02}},
	{0x5F, 01, {0x01}},
	//120hz odc on
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xE0, 01, {0x01}},
	{0xE2, 04, {0xD6,0x4E,0x82,0x88}},
	{0x6F, 01, {0x12}},
	{0xE2, 02, {0xFD,0x03}},
	{0xEA,  8, {0x02,0x00,0xE0,0x00,0xE0,0x80,0x00,0x80}},
	{0x6F, 01, {0x17}},
	{0xEA, 03, {0x38,0x00,0x38}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_120hz_setting_gir_on[] = {
	/* frequency select 120hz */
	{0x2F, 01, {0x05}},
	{0x5F, 01, {0x00}},
	//120hz odc on
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xE0, 01, {0x01}},
	{0xE2, 04, {0xD6,0x4E,0x82,0x88}},
	{0x6F, 01, {0x12}},
	{0xE2, 02, {0xFD,0x03}},
	{0xEA,  8, {0x02,0x00,0xE0,0x00,0xE0,0x80,0x00,0x80}},
	{0x6F, 01, {0x17}},
	{0xEA, 03, {0x38,0x00,0x38}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_90hz_setting_gir_off[] = {
	/* frequency select 90hz */
	{0x44, 02, {0x00, 0x00}},
	{0x2F, 01, {0x01}},
	{0x5F, 01, {0x01}},
	//90hz odc setting
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xE0, 01, {0x01}},
	{0xE2, 04, {0x42,0xEE,0x04,0x20}},
	{0x6F, 01, {0x12}},
	{0xE2, 02, {0xFD,0x03}},
	{0xEA,  8, {0x02,0x00,0xB8,0x00,0xB8,0x58,0x00,0x58}},
	{0x6F, 01, {0x17}},
	{0xEA, 03, {0x48,0x00,0x48}},
	{REGFLAG_END_OF_TABLE, 0x01, {}}
};

static struct LCM_setting_table mode_90hz_setting_gir_on[] = {
	/* frequency select 90hz */
	{0x44, 02, {0x00, 0x00}},
	{0x2F, 01, {0x04}},
	{0x5F, 01, {0x00}},
	//90hz odc setting
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xE0, 01, {0x01}},
	{0xE2, 04, {0x42,0xEE,0x04,0x20}},
	{0x6F, 01, {0x12}},
	{0xE2, 02, {0xFD,0x03}},
	{0xEA,  8, {0x02,0x00,0xB8,0x00,0xB8,0x58,0x00,0x58}},
	{0x6F, 01, {0x17}},
	{0xEA, 03, {0x48,0x00,0x48}},
	{REGFLAG_END_OF_TABLE, 0x01, {}}
};

static struct LCM_setting_table mode_60hz_setting_gir_off[] = {
	/* frequency select 60hz */
	{0x2F, 01, {0x00}},
	{0x5F, 01, {0x01}},
	//60hz odc setting
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xE0, 01, {0x01}},
	{0xE2, 04, {0x88,0xE0,0xCC,0x0C}},
	{0x6F, 01, {0x12}},
	{0xE2, 02, {0x86,0x01}},
	{0xEA,  8, {0x02,0x00,0xE0,0x00,0xE0,0x70,0x00,0x70}},
	{0x6F, 01, {0x17}},
	{0xEA, 03, {0x80,0x00,0x80}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table mode_60hz_setting_gir_on[] = {
	/* frequency select 60hz */
	{0x2F, 01, {0x03}},
	{0x5F, 01, {0x00}},
	//60hz odc setting
	{0xF0, 05, {0x55,0xAA,0x52,0x08,0x08}},
	{0xE0, 01, {0x01}},
	{0xE2, 04, {0x88,0xE0,0xCC,0x0C}},
	{0x6F, 01, {0x12}},
	{0xE2, 02, {0x86,0x01}},
	{0xEA,  8, {0x02,0x00,0xE0,0x00,0xE0,0x70,0x00,0x70}},
	{0x6F, 01, {0x17}},
	{0xEA, 03, {0x80,0x00,0x80}},
	{REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void mode_switch_to_120(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_120hz_setting_gir_on,
				sizeof(mode_120hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_120hz_setting_gir_off,
				sizeof(mode_120hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 120;
	}

}

static void mode_switch_to_90(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_90hz_setting_gir_on,
				sizeof(mode_90hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_90hz_setting_gir_off,
				sizeof(mode_90hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 90;
	}

}


static void mode_switch_to_60(struct drm_panel *panel,
	enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct lcm *ctx = panel_to_lcm(panel);
	pr_info("%s state = %d\n", __func__, stage);
	if (stage == BEFORE_DSI_POWERDOWN) {
		if (ctx->gir_status)
			push_table(ctx, mode_60hz_setting_gir_on,
				sizeof(mode_60hz_setting_gir_on) / sizeof(struct LCM_setting_table));
		else
			push_table(ctx, mode_60hz_setting_gir_off,
				sizeof(mode_60hz_setting_gir_off) / sizeof(struct LCM_setting_table));
		ctx->dynamic_fps = 60;
	}

}

static int mode_switch(struct drm_panel *panel,
		struct drm_connector *connector, unsigned int cur_mode,
		unsigned int dst_mode, enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	int ret = 0;
	bool isFpsChange = false;

	struct drm_display_mode *m_dst = get_mode_by_id(connector, dst_mode);
	struct drm_display_mode *m_cur = get_mode_by_id(connector, cur_mode);

	if (cur_mode == dst_mode)
		return ret;

	isFpsChange = drm_mode_vrefresh(m_dst) == drm_mode_vrefresh(m_cur)? false: true;

	pr_info("%s isFpsChange = %d\n", __func__, isFpsChange);
	pr_info("%s dst_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_dst), m_dst->vdisplay, m_dst->hdisplay);
	pr_info("%s cur_mode vrefresh = %d, vdisplay = %d, vdisplay = %d\n",
		__func__, drm_mode_vrefresh(m_cur), m_cur->vdisplay, m_cur->hdisplay);

	if (isFpsChange) {
		if (drm_mode_vrefresh(m_dst) == MODE0_FPS)
			mode_switch_to_60(panel, stage);
		else if (drm_mode_vrefresh(m_dst) == MODE2_FPS)
			mode_switch_to_90(panel, stage);
		else if (drm_mode_vrefresh(m_dst) == MODE1_FPS)
			mode_switch_to_120(panel, stage);
		else
			ret = 1;
	}

	return ret;
}

#ifdef CONFIG_MI_DISP
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
		pr_err("invalid params\n");
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
	int ret = 0;
#ifdef CONFIG_FACTORY_BUILD
	char backlight_h[] = {0x51, 0x07, 0xFF, 0x07, 0xFF, 0x03, 0xFF};
	char backlight_l[] = {0x51, 0x07, 0xFF, 0x07, 0xFF, 0x01, 0xFF};
	char backlight_0[] = {0x51, 0x07, 0xFF, 0x07, 0xFF, 0x00, 0x00};
#else
	char backlight_h[] = {0x51, 0x00, 0xF6, 0x00, 0xF6, 0x03, 0xFF};
	char backlight_l[] = {0x51, 0x00, 0x14, 0x00, 0x14, 0x01, 0xFF};
	char backlight_0[] = {0x51, 0x00, 0x08, 0x00, 0x08, 0x00, 0x00};
#endif

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 1,
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

	if (!atomic_read(&doze_enable)) {
		pr_info("%s normal mode cannot set doze brightness\n", __func__);
		goto exit;
	}

	if (DOZE_TO_NORMAL == doze_brightness) {
		cmd_msg.type[0] = ARRAY_SIZE(backlight_0) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[0] = (backlight_0);
		cmd_msg.tx_len[0] = ARRAY_SIZE((backlight_0));
		mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
		atomic_set(&doze_enable, 0); 
		goto exit;
	} else if (DOZE_BRIGHTNESS_LBM  == doze_brightness) {
		cmd_msg.type[0] = ARRAY_SIZE(backlight_l) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[0] = backlight_l;
		cmd_msg.tx_len[0] = ARRAY_SIZE(backlight_l);

	} else if (DOZE_BRIGHTNESS_HBM == doze_brightness) {
		cmd_msg.type[0] = ARRAY_SIZE(backlight_h) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[0] = backlight_h;
		cmd_msg.tx_len[0] = ARRAY_SIZE(backlight_h);
	}

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}

exit :
	ctx->doze_brightness_state = doze_brightness;
	ctx->doze_suspend = false;
	pr_info("%s end -\n", __func__);
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
	char crc_level_write_cmd4[] = {0xB0, 0x00, 0x02, 0x92};
	char crc_level_write_cmd5[] = {0x92, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF};
	char crc_level_write_cmd6[] = {0xB0, 0x00, 0x2C, 0x92};
	char crc_level_write_cmd7[] = {0x92, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF};
	char crc_level_write_cmd8[] = {0xB0, 0x00, 0x56, 0x92};
	char crc_level_write_cmd9[] = {0x92, 0x16};
	char crc_level_write_cmd10[] = {0xF0, 0xA5, 0xA5};
	char crc_level_write_cmd11[] = {0x51, 0x00, 0x00};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 12,
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
		crc_level_write_cmd11[1] = (gDcThreshold >> 8) & 0xFF;
		crc_level_write_cmd11[2] = gDcThreshold & 0xFF;
	} else {
		crc_level = 255.0;
	}
	pr_info("%s crc_level = %d", __func__, crc_level);
	if (ctx->crc_level == crc_level) {
		ret = 0;
		pr_info("%s crc_level not change, skip set crc", __func__);
		goto err;
	}

	for (i = 1; i < ARRAY_SIZE(crc_level_write_cmd7); i++) {
		if (crc_level_write_cmd7[i] != 0)
			crc_level_write_cmd7[i] = crc_level & 0xFF;
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

	cmd_msg.type[10] = ARRAY_SIZE(crc_level_write_cmd10) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[10] = crc_level_write_cmd10;
	cmd_msg.tx_len[10] = ARRAY_SIZE(crc_level_write_cmd10);

	cmd_msg.type[11] = ARRAY_SIZE(crc_level_write_cmd11) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[11] = crc_level_write_cmd11;
	cmd_msg.tx_len[11] = ARRAY_SIZE(crc_level_write_cmd11);

	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, false, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}

	ctx->crc_level = crc_level;

err:
	pr_info("%s: -\n", __func__);
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
	char gir_on_write_cmd0[] = {0x5F, 0x00};
	char gir_on_write_cmd1[] = {0x2F, 0x00};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 2,
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

	if (ctx->dynamic_fps == 60) {
		gir_on_write_cmd1[1] = 0x03;
	} else if (ctx->dynamic_fps == 90) {
		gir_on_write_cmd1[1] = 0x04;
	} else if (ctx->dynamic_fps == 120) {
		gir_on_write_cmd1[1] = 0x05;
	}

	cmd_msg.type[1] = ARRAY_SIZE(gir_on_write_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = gir_on_write_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(gir_on_write_cmd1);

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
	char gir_off_write_cmd0[] = {0x5F, 0x01};
	char gir_off_write_cmd1[] = {0x2F, 0x00};

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
	ctx->gir_status = 0;
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		goto err;
	}

	cmd_msg.type[0] = ARRAY_SIZE(gir_off_write_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = gir_off_write_cmd1;
	cmd_msg.tx_len[0] = ARRAY_SIZE(gir_off_write_cmd1);

	if (ctx->dynamic_fps == 60) {
		gir_off_write_cmd1[1] = 0x00;
	} else if (ctx->dynamic_fps == 90) {
		gir_off_write_cmd1[1] = 0x01;
	} else if (ctx->dynamic_fps == 120) {
		gir_off_write_cmd1[1] = 0x02;
	}

	cmd_msg.type[1] = ARRAY_SIZE(gir_off_write_cmd1) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[1] = gir_off_write_cmd1;
	cmd_msg.tx_len[1] = ARRAY_SIZE(gir_off_write_cmd1);

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
	char dimming_control_cmd[] = {0x53, 0x20};

	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 1,
	};

	if (!panel) {
		pr_err("invalid params\n");
		return;
	}

	pr_info("%s dimming = %d\n", __func__, en);
	ctx = panel_to_lcm(panel);

	if (en)
		dimming_control_cmd[1] = 0x28;
	else
		dimming_control_cmd[1] = 0x20;

	cmd_msg.type[0] = ARRAY_SIZE(dimming_control_cmd) > 2 ? 0x39 : 0x15;
	cmd_msg.tx_buf[0] = dimming_control_cmd;
	cmd_msg.tx_len[0] = ARRAY_SIZE(dimming_control_cmd);

	ret = mtk_ddic_dsi_wait_te_send_cmd(&cmd_msg, true);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
	pr_info("%s end -\n", __func__);
}

static int panel_hbm_fod_control(struct drm_panel *panel, bool en)
{
	struct lcm *ctx;
	int ret = -1;
	char hbm_fod_on_cmd[] = {0x51, 0x0F, 0xFF};
	char hbm_fod_off_cmd[] = {0x51, 0x07, 0xFF};
	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = 1,
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
		cmd_msg.type[0] = ARRAY_SIZE(hbm_fod_on_cmd) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[0] = (hbm_fod_on_cmd);
		cmd_msg.tx_len[0] = ARRAY_SIZE((hbm_fod_on_cmd));
	} else {
		hbm_fod_off_cmd[1] = bl_tb0[1];
		hbm_fod_off_cmd[2] = bl_tb0[2];

		cmd_msg.type[0] = ARRAY_SIZE(hbm_fod_off_cmd) > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[0] = hbm_fod_off_cmd;
		cmd_msg.tx_len[0] = ARRAY_SIZE(hbm_fod_off_cmd);
	}
	ret = mtk_ddic_dsi_send_cmd(&cmd_msg, true, false);
	if (ret != 0) {
		DDPPR_ERR("%s: failed to send ddic cmd\n", __func__);
	}
err:
	pr_info("%s: -\n", __func__);
	return ret;
}

int panel_get_grayscale_info(struct drm_panel *panel, char *buf, size_t size)
{
	int i;
	int index = 0, len =0;
	static u8 grayscale_cmdline[33] = {0};

	pr_info("%s: +\n", __func__);

	if (!panel) {
		pr_err("invalid params\n");
		return -EAGAIN;
	}

	pr_info("%s: oled_grayscale_cmdline:%s\n", __func__, oled_grayscale_cmdline);

	for (i = 0; i < 33; i++) {
		sscanf(oled_grayscale_cmdline + 2 * i, "%02hhx", &grayscale_cmdline[i]);
	}

	pr_info("%s:  buf size:%d \n", __func__, size);
	for (i = 0; i < 32; i++) {
		len = snprintf(&buf[index], size, "0x%02x,", grayscale_cmdline[i]);
		index += len;
	}

	len = snprintf(&buf[index], size, "0x%02x", grayscale_cmdline[32]);
	index += len;
	pr_info("%s: grayscale_cmdline:%s\n", __func__, grayscale_cmdline);

	return index;
}


int panel_get_wp_info(struct drm_panel *panel, char *buf, size_t size)
{
	/* Read 3 bytes from 0xDF register
	 * 	BIT[0] = Lux
	 * 	BIT[1] = Wx
	 * 	BIT[2] = Wy */
	static uint16_t lux = 0, wx = 0, wy = 0;
	int ret = 0, count = 0;
	struct lcm *ctx = NULL;
	u8 page3[] = {0xF0, 0x55, 0xAA, 0x52, 0x08, 0x03};
	u8 tx_buf[1] = {0xDF};
	u8 rx_buf[6] = {0x00};
	struct mtk_ddic_dsi_msg cmds[] = {
		{
			.channel = 1,
			.flags = 2,
			.tx_cmd_num = 1,
			.type[0] = 0x39,
			.tx_buf[0] = &page3,
			.tx_len[0] = ARRAY_SIZE(page3),
		},
		{
			.channel = 0,
			.tx_cmd_num = 1,
			.rx_cmd_num = 1,
			.type[0] = 0x06,
			.tx_buf[0] = &tx_buf[0],
			.tx_len[0] = 1,
			.rx_buf[0] = &rx_buf[0],
			.rx_len[0] = 3,
		},
	};

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
	ret = mtk_ddic_dsi_send_cmd(&cmds[0], true, false);
	ret |= mtk_ddic_dsi_read_cmd(&cmds[1]);
	if (ret != 0) {
		pr_err("%s: failed to read ddic register\n", __func__);
		memset(rx_buf, 0, sizeof(rx_buf));
		goto err;
	}

	/* lux min is 308nit, wx min is 0.172, wy min is 0.187 */
	lux = ((3080 + rx_buf[0] * 15 ) > 6905 ? 6905 : (3080 + rx_buf[0] * 15)) / 10;
	wx = (172 + rx_buf[1]) > 427 ? 427 : (172 + rx_buf[1]);
	wy = (187 + rx_buf[2]) > 442 ? 442 : (187 + rx_buf[2]);

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
	atomic_set(&doze_enable, 1);
	pr_info("%s !-\n", __func__);
	return 0;
}

static int panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
		char esd_on_page1[] = {0xF0,0x55,0xAA,0x52,0x08,0x00};
		char esd_on_page2[] = {0x6F,0x01};
		char esd_on_page3[] = {0xBE,0x45};
		char aod2normal_tb[] = {0x65, 0x00};
		char offcmd0_tb[] = {0x38, 0x00};
		char page_51[] = {0x51, 0x00, 0x08};
		char offcmd1_tb[] = {0x2C, 0x00};
		char lhbm_off_page[] = {0x87, 0x00};

		pr_info("%s +\n", __func__);

		cb(dsi, handle, esd_on_page1, ARRAY_SIZE(esd_on_page1));
		cb(dsi, handle, esd_on_page2, ARRAY_SIZE(esd_on_page2));
		cb(dsi, handle, esd_on_page3, ARRAY_SIZE(esd_on_page3));
		cb(dsi, handle, aod2normal_tb, ARRAY_SIZE(aod2normal_tb));
		cb(dsi, handle, offcmd0_tb, ARRAY_SIZE(offcmd0_tb));
		cb(dsi, handle, page_51, ARRAY_SIZE(page_51));
		cb(dsi, handle, offcmd1_tb, ARRAY_SIZE(offcmd1_tb));
		usleep_range(40 * 1000, 40 * 1000 + 10);
		atomic_set(&doze_enable, 0);
		cb(dsi, handle, lhbm_off_page, ARRAY_SIZE(lhbm_off_page));

		pr_info("%s -\n", __func__);

		return 0;
}

static void mi_disp_panel_ddic_send_cmd(struct LCM_setting_table *table,
	unsigned int count, bool wait_te_send, bool block)
{
	int i, j, ret;
	struct mtk_ddic_dsi_msg cmd_msg = {
		.channel = 1,
		.flags = 2,
		.tx_cmd_num = count,
	};

	if (table == NULL) {
		pr_err("invalid ddic cmd \n");
		return;
	}

	if (count == 0 || count > 25) {
		pr_err("cmd count invalid, value:%d \n", count);
		return;
	}

	pr_info("mi disp send cmd,wait_te:%d , conut:%d\n", wait_te_send, count);
	for (i = 0;i < count; i++) {
		cmd_msg.type[i] = table[i].count > 2 ? 0x39 : 0x15;
		cmd_msg.tx_buf[i] = table[i].para_list;
		cmd_msg.tx_len[i] = table[i].count;
		pr_info("cmd count:%d, cmd_add:%x len:%d\n", count, table[i].cmd,table[i].count);
		for (j = 0;j < table[i].count; j++)
			pr_info("0x%02hhx ",table[i].para_list[j]);
	}

	if (wait_te_send)
		ret = mtk_ddic_dsi_wait_te_send_cmd(&cmd_msg, block);
	else
		ret = mtk_ddic_dsi_send_cmd(&cmd_msg, block, false);
	
	if (ret != 0) {
		pr_err("%s: failed to send ddic cmd\n", __func__);
	}
	return;
}

static void mi_parse_cmdline_perBL(struct LHBM_WHITEBUF * lhbm_whitebuf) {
	int i = 0, temp = 0;
	int gamma_coffee_w900[3] = {109,108,109};
	static u16 lhbm_cmdbuf[14] = {0};

	pr_info("mi_parse_cmdline_perBL enter\n");

	if(!lhbm_w900_update_flag && !lhbm_w110_update_flag && !lhbm_g500_update_flag) {
		pr_info("don't need update white rgb config");
		return;
	}

	if (lhbm_whitebuf == NULL) {
		pr_err("lhbm_status == NULL\n");
		return;
	}
	for (i = 0; i < 14; i++) {
		sscanf(oled_lhbm_cmdline + 4 * i, "%04hx", &lhbm_cmdbuf[i]);
	}
/*
	for (i = 0; i < 6; i++){
		lhbm_whitebuf->gir_off_110[i] = lhbm_cmdbuf[i];
		lhbm_whitebuf->gir_on_110[i] = lhbm_cmdbuf[i+6];
		lhbm_whitebuf->gir_off_900[i] = lhbm_cmdbuf[i+12];
		lhbm_whitebuf->gir_on_900[i] = lhbm_cmdbuf[i+18];
	}

	for (i = 0; i < 2; i++) {
		lhbm_whitebuf->gir_off_500[i] = lhbm_cmdbuf[i+24];
		lhbm_whitebuf->gir_on_500[i] = lhbm_cmdbuf[i+26];
	}
*/
	for (i = 0; i < 6; i += 2) {
		//110nit gir off
		temp = (((int)lhbm_cmdbuf[i/2]) * 103) / 100;
		lhbm_whitebuf->gir_off_110[i] = (temp & 0xFF00) >> 8;
		lhbm_whitebuf->gir_off_110[i+1] = temp & 0xFF;

		//110nit gir on
		temp = (((int)lhbm_cmdbuf[i/2 + 3]) * 103) / 100;
		lhbm_whitebuf->gir_on_110[i] = (temp & 0xFF00) >> 8;
		lhbm_whitebuf->gir_on_110[i+1] = temp & 0xFF;

		//900nit gir off
		temp = (((int)lhbm_cmdbuf[i/2 + 6]) *
			gamma_coffee_w900[i/2]) / 100;
		lhbm_whitebuf->gir_off_900[i] = (temp & 0xFF00) >> 8;
		lhbm_whitebuf->gir_off_900[i+1] = temp & 0xFF;

		//900nit gir off
		temp = (((int)lhbm_cmdbuf[i/2 + 9]) *
			gamma_coffee_w900[i/2]) / 100;
		lhbm_whitebuf->gir_on_900[i] = (temp & 0xFF00) >> 8;
		lhbm_whitebuf->gir_on_900[i+1] = temp & 0xFF;
	}

	//w500nit gir off
	temp = (((int)lhbm_cmdbuf[12]) * 103) / 100;
	lhbm_whitebuf->gir_off_500[0] = (temp & 0xFF00) >> 8;
	lhbm_whitebuf->gir_off_500[1] = temp & 0xFF;

	//w500nit gir on
	temp = (((int)lhbm_cmdbuf[13]) * 103) / 100;
	lhbm_whitebuf->gir_on_500[0] = (temp & 0xFF00) >> 8;
	lhbm_whitebuf->gir_on_500[1] = temp & 0xFF;

	pr_info("gir_off_110 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx",lhbm_whitebuf->gir_off_110[i]);
	}

	pr_info("gir_on_110 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx ",lhbm_whitebuf->gir_on_110[i]);
	}

	pr_info("gir_off_900 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx ",lhbm_whitebuf->gir_off_900[i]);
	}

	pr_info("gir_on_900 \n");
	for (i = 0; i < 6; i++){
		pr_info("0x%02hhx ",lhbm_whitebuf->gir_on_900[i]);
	}
	pr_info("gir 500 0x%02hhx 0x%02hhx 0x%02hhx 0x%02hhx,\n",
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
	struct lcm * ctx = NULL;

	if(!dsi) {
		pr_err("dsi is null\n");
		return -EINVAL;
	}

	ctx = panel_to_lcm(dsi->panel);
	if(!ctx) {
		pr_err("ctx is null\n");
		return -EINVAL;
	}
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
	case TYPE_WHITE_900:
		if (flat_mode) {
			for (i = 0; i < 6; i++) {
				lhbm_normal_white_900nit[4].para_list[i+1] = lhbm_whitebuf.gir_on_900[i];
			}

			lhbm_normal_white_900nit[2].para_list[1] =
				ctx->dynamic_fps == 60 ? 0x23 : 0x25;
		} else {
			for (i = 0; i < 6; i++) {
				lhbm_normal_white_900nit[4].para_list[i+1] = lhbm_whitebuf.gir_off_900[i];
			}

			lhbm_normal_white_900nit[2].para_list[1] =
				ctx->dynamic_fps == 60 ? 0x20 : 0x22;
		}
		break;
	case TYPE_WHITE_110:
		if (flat_mode) {
			for (i = 0; i < 6; i++) {
				lhbm_normal_white_110nit[4].para_list[i+1] = lhbm_whitebuf.gir_on_110[i];
			}

			lhbm_normal_white_110nit[2].para_list[1] =
				ctx->dynamic_fps == 60 ? 0x23 : 0x25;
		} else {
			for (i = 0; i < 6; i++) {
				lhbm_normal_white_110nit[4].para_list[i+1] = lhbm_whitebuf.gir_off_110[i];
			}

			lhbm_normal_white_110nit[2].para_list[1] =
				ctx->dynamic_fps == 60 ? 0x20 : 0x22;
		}
		break;
	case TYPE_GREEN_500:
		if (flat_mode) {
			for (i = 0; i < 2; i++) {
				lhbm_normal_green_500nit[4].para_list[i+3] = lhbm_whitebuf.gir_on_500[i];
			}

			lhbm_normal_green_500nit[2].para_list[1] =
				ctx->dynamic_fps == 60 ? 0x23 : 0x25;
		} else {
			for (i = 0; i < 2; i++) {
				lhbm_normal_green_500nit[4].para_list[i+3] = lhbm_whitebuf.gir_off_500[i];
			}

			lhbm_normal_green_500nit[2].para_list[1] =
				ctx->dynamic_fps == 60 ? 0x20 : 0x22;
		}
		break;
	case TYPE_HLPM_W900:
		if (flat_mode) {
			for (i = 0; i < 6; i++) {
				lhbm_hlpm_white_900nit[8].para_list[i+1] = lhbm_whitebuf.gir_on_900[i];
			}

			lhbm_hlpm_white_900nit[6].para_list[1] =
				ctx->dynamic_fps == 60 ? 0x23 : 0x25;
		} else {
			for (i = 0; i < 6; i++) {
				lhbm_hlpm_white_900nit[8].para_list[i+1] = lhbm_whitebuf.gir_off_900[i];
			}

			lhbm_hlpm_white_900nit[6].para_list[1] =
				ctx->dynamic_fps == 60 ? 0x20 : 0x22;
		}
		break;
	case TYPE_HLPM_W110:
		if (flat_mode) {
			for (i = 0; i < 6; i++) {
				lhbm_hlpm_white_110nit[8].para_list[i+1] = lhbm_whitebuf.gir_on_110[i];
			}

			lhbm_hlpm_white_110nit[6].para_list[1] =
				ctx->dynamic_fps == 60 ? 0x23 : 0x25;
		} else {
			for (i = 0; i < 6; i++) {
				lhbm_hlpm_white_110nit[8].para_list[i+1] = lhbm_whitebuf.gir_off_110[i];
			}

			lhbm_hlpm_white_110nit[6].para_list[1] =
				ctx->dynamic_fps == 60 ? 0x20 : 0x22;
		}
		break;
	default:
		pr_err("unsuppport cmd \n");
	return -EINVAL;
	}

	return 0;
}

static void mi_disp_panel_update_lhbm_backlight(struct mtk_dsi *dsi,enum lhbm_cmd_type type , int bl_level) {
	u8 backlight_buf[2] = {0};
	struct lcm *ctx = NULL;

	if (!dsi || type >= TYPE_MAX) {
		pr_err("invalid params\n");
		return;
	}

	ctx = panel_to_lcm(dsi->panel);
	pr_info("%s [%d] bl_lvl = %d,\n", __func__, type, bl_level);

	backlight_buf[0] = bl_level >> 8;
	backlight_buf[1] = bl_level & 0xFF;

	switch (type) {
	case TYPE_HLPM_W900:
		lhbm_hlpm_white_900nit[2].para_list[1] = backlight_buf[0];
		lhbm_hlpm_white_900nit[2].para_list[2] = backlight_buf[1];
		lhbm_hlpm_white_900nit[2].para_list[3] = backlight_buf[0];
		lhbm_hlpm_white_900nit[2].para_list[4] = backlight_buf[1];
		if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
			lhbm_hlpm_white_900nit[2].para_list[5] = 0x03;
		else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
			lhbm_hlpm_white_900nit[2].para_list[5] = 0x01;
		break;
	case TYPE_HLPM_W110:
		lhbm_hlpm_white_110nit[2].para_list[1] = backlight_buf[0];
		lhbm_hlpm_white_110nit[2].para_list[2] = backlight_buf[1];
		lhbm_hlpm_white_110nit[2].para_list[3] = backlight_buf[0];
		lhbm_hlpm_white_110nit[2].para_list[4] = backlight_buf[1];
		if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM)
			lhbm_hlpm_white_110nit[2].para_list[5] = 0x03;
		else if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
			lhbm_hlpm_white_110nit[2].para_list[5] = 0x01;
		break;
	default:
		pr_err("unsupport update backlight type\n");
		return;
	}
}

static void mi_disp_panel_update_lhbm_alpha(struct mtk_dsi *dsi,enum lhbm_cmd_type type , int bl_level) {
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

	pr_info("%s [%d] bl_lvl = %d,\n", __func__, type, bl_level);

	if (!(type == TYPE_LHBM_OFF || type == TYPE_HLPM_OFF)) {
		if (bl_level < 0x148) {
			if (bl_level <= 8) {
				pr_info("[%d] lhbm bl_level too low, set bl_level 8\n", type);
				bl_level = 8;
			}
			alpha_buf[0] = (aa_alpha_set_tm[bl_level] >> 8) & 0x0f;
			alpha_buf[1] = aa_alpha_set_tm[bl_level] & 0xff;
		} else {
			if (bl_level > 2047)
				bl_level = 2047;
			alpha_buf[0] = (bl_level * 4) >> 8;
			alpha_buf[1] = (bl_level * 4) & 0xFF;
		}

		pr_info("%s alpha_buf[0]:%d alpha_buf[1]:%d\n", __func__, alpha_buf[0], alpha_buf[1]);
	}

	switch (type) {
	case TYPE_WHITE_900:
		if (bl_level < 0x148) {
			pr_info("%s 0x000~0x148 w900 update 0x87\n", __func__);

			if (lhbm_normal_white_900nit[9].para_list[0] != 0x87) {
				pr_err("update w900 0x87 error\n");
				return;
			}
			lhbm_normal_white_900nit[9].para_list[2] = alpha_buf[0];
			lhbm_normal_white_900nit[9].para_list[3] = alpha_buf[1];

			//update 0xDF,0x05,0x20 in 0x000~0x148
			lhbm_normal_white_900nit[7].para_list[1] = 0x05;
			lhbm_normal_white_900nit[7].para_list[2] = 0x20;
		} else {
			pr_info("%s 0x148~0x7FF w900 update 0xDF\n", __func__);

			if (lhbm_normal_white_900nit[7].para_list[0] != 0xDF) {
				pr_err("update w900 0xDF error\n");
				return;
			}
			lhbm_normal_white_900nit[7].para_list[1] = alpha_buf[0];
			lhbm_normal_white_900nit[7].para_list[2] = alpha_buf[1];

			//update 0x87 0x25,0x7f,0xff in 0x148~0x7FF
			lhbm_normal_white_900nit[9].para_list[2] = 0x0F;
			lhbm_normal_white_900nit[9].para_list[3] = 0xFF;
		}
		break;
	case TYPE_WHITE_110:
		if (bl_level < 0x148) {
			pr_info("%s 0x000~0x148 w110 update 0x87\n", __func__);

			if (lhbm_normal_white_110nit[9].para_list[0] != 0x87) {
				pr_err("update w110 0x87 error\n");
				return;
			}
			lhbm_normal_white_110nit[9].para_list[2] = alpha_buf[0];
			lhbm_normal_white_110nit[9].para_list[3] = alpha_buf[1];

			//update 0xDF,0x05,0x20 in 0x000~0x148
			lhbm_normal_white_110nit[7].para_list[1] = 0x05;
			lhbm_normal_white_110nit[7].para_list[2] = 0x20;
		} else {
			pr_info("%s 0x148~0x7FF w110 update 0xDF\n", __func__);

			if (lhbm_normal_white_110nit[7].para_list[0] != 0xDF) {
				pr_err("update w900 0xDF error\n");
				return;
			}
			lhbm_normal_white_110nit[7].para_list[1] = alpha_buf[0];
			lhbm_normal_white_110nit[7].para_list[2] = alpha_buf[1];

			//update 0x87 0x25,0x7f,0xff in 0x148~0x7FF
			lhbm_normal_white_110nit[9].para_list[2] = 0x0F;
			lhbm_normal_white_110nit[9].para_list[3] = 0xFF;
		}
		break;
	case TYPE_GREEN_500:
		if (bl_level < 0x148) {
			pr_info("%s 0x000~0x148 g500 update 0x87\n", __func__);

			if (lhbm_normal_white_110nit[9].para_list[0] != 0x87) {
				pr_err("update w110 0x87 error\n");
				return;
			}
			lhbm_normal_green_500nit[9].para_list[2] = alpha_buf[0];
			lhbm_normal_green_500nit[9].para_list[3] = alpha_buf[1];

			//update 0xDF,0x05,0x20 in 0x000~0x148
			lhbm_normal_green_500nit[7].para_list[1] = 0x05;
			lhbm_normal_green_500nit[7].para_list[2] = 0x20;
		} else {
			pr_info("%s 0x148~0x7FF g500 update 0xDF\n", __func__);

			if (lhbm_normal_green_500nit[7].para_list[0] != 0xDF) {
				pr_err("update g500 0xDF error\n");
				return;
			}
			lhbm_normal_green_500nit[7].para_list[1] = alpha_buf[0];
			lhbm_normal_green_500nit[7].para_list[2] = alpha_buf[1];

			//update 0x87 0x25,0x7f,0xff in 0x148~0x7FF
			lhbm_normal_green_500nit[9].para_list[2] = 0x0F;
			lhbm_normal_green_500nit[9].para_list[3] = 0xFF;
		}

		break;
	case TYPE_HLPM_OFF:
	case TYPE_LHBM_OFF:
		alpha_buf[0] = bl_level >> 8;
		alpha_buf[1] = bl_level & 0xFF;

		pr_info("TYPE_LHBM_OFF restore backlight:%d\n", bl_level);
		if (lhbm_off[0].para_list[0] != 0x51) {
			pr_err("update g500 0x51 error\n");
			return;
		}
		lhbm_off[0].para_list[1] = alpha_buf[0];
		lhbm_off[0].para_list[2] = alpha_buf[1];
		break;
	case TYPE_HLPM_W900:
		if (bl_level < 0x148) {
			pr_info("%s 0x000~0x148 hlpm900 update 0x87\n", __func__);

			if (lhbm_hlpm_white_900nit[13].para_list[0] != 0x87) {
				pr_err("update hlpm900 0x87 error\n");
				return;
			}
			lhbm_hlpm_white_900nit[13].para_list[2] = alpha_buf[0];
			lhbm_hlpm_white_900nit[13].para_list[3] = alpha_buf[1];

			//update 0xDF,0x05,0x20 in 0x000~0x148
			lhbm_hlpm_white_900nit[11].para_list[1] = 0x05;
			lhbm_hlpm_white_900nit[11].para_list[2] = 0x20;
		} else {
			pr_info("%s 0x148~0x7FF hlpm900 update 0xDF\n", __func__);

			if (lhbm_hlpm_white_900nit[11].para_list[0] != 0xDF) {
				pr_err("update hlpm900 0xDF error\n");
				return;
			}
			lhbm_hlpm_white_900nit[11].para_list[1] = alpha_buf[0];
			lhbm_hlpm_white_900nit[11].para_list[2] = alpha_buf[1];

			//update 0x87 0x25,0x7f,0xff in 0x148~0x7FF
			lhbm_hlpm_white_900nit[13].para_list[2] = 0x0F;
			lhbm_hlpm_white_900nit[13].para_list[3] = 0xFF;
		}
		break;
	case TYPE_HLPM_W110:
		if (bl_level < 0x148) {
			pr_info("%s 0x000~0x148 hlpm110 update 0x87\n", __func__);

			if (lhbm_hlpm_white_110nit[13].para_list[0] != 0x87) {
				pr_err("update hlpm110 0x87 error\n");
				return;
			}
			lhbm_hlpm_white_110nit[13].para_list[2] = alpha_buf[0];
			lhbm_hlpm_white_110nit[13].para_list[3] = alpha_buf[1];

			//update 0xDF,0x05,0x20 in 0x000~0x148
			lhbm_hlpm_white_110nit[11].para_list[1] = 0x05;
			lhbm_hlpm_white_110nit[11].para_list[2] = 0x20;
		} else {
			pr_info("%s 0x148~0x7FF hlpm110 update 0xDF\n", __func__);

			if (lhbm_hlpm_white_110nit[11].para_list[0] != 0xDF) {
				pr_err("update hlpm110 0xDF error\n");
				return;
			}
			lhbm_hlpm_white_110nit[11].para_list[1] = alpha_buf[0];
			lhbm_hlpm_white_110nit[11].para_list[2] = alpha_buf[1];

			//update 0x87 0x25,0x7f,0xff in 0x148~0x7FF
			lhbm_hlpm_white_110nit[13].para_list[2] = 0x0F;
			lhbm_hlpm_white_110nit[13].para_list[3] = 0xFF;
		}

		break;
	default:
		pr_err("unsupport update 0x87 type\n");
		return;
	}

	pr_info("mi_disp_panel_update_lhbm_alpha end\n");
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
	if (!ctx->enabled) {
		pr_err("%s: panel isn't enabled\n", __func__);
		return -1;
	}

	mi_cfg = &dsi->mi_cfg;
	bl_level = mi_cfg->last_bl_level;
	flat_mode = mi_cfg->feature_val[DISP_FEATURE_FLAT_MODE];

	if (atomic_read(&doze_enable)) {
		if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_LBM)
			bl_level_doze = 20;
		pr_info("%s doze mode bl_lvl :%d \n", __func__, bl_level_doze);
	}
	pr_info("%s local hbm_state :%d \n", __func__, lhbm_state);
	pr_info("bl_level:%d  flat_mode:%d\n", bl_level, flat_mode);
	switch (lhbm_state) {
	case LOCAL_HBM_OFF_TO_NORMAL:
		pr_info("LOCAL_HBM_NORMAL off\n");
		mi_disp_panel_update_lhbm_alpha(dsi, TYPE_LHBM_OFF, bl_level);
		mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), false, true);
		break;
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT_RESTORE:
	case LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT:
		pr_info("LOCAL_HBM_OFF_TO_NORMAL_BACKLIGHT\n");
		if (atomic_read(&doze_enable)) {
			if (bl_level)
				mi_disp_panel_update_lhbm_alpha(dsi, TYPE_LHBM_OFF, bl_level);
			else 
				mi_disp_panel_update_lhbm_alpha(dsi, TYPE_LHBM_OFF, bl_level_doze);
		} else {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_LHBM_OFF, bl_level);
		}
		mi_disp_panel_ddic_send_cmd(lhbm_off, ARRAY_SIZE(lhbm_off), false, true);
		break;
	case LOCAL_HBM_NORMAL_WHITE_750NIT:
	case LOCAL_HBM_NORMAL_WHITE_500NIT:
		break;
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
		if (bl_level > 0x147)
			pr_info("warn lhbm w110 and bl > 0x147147\n");
		if (atomic_read(&doze_enable)) {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W110, bl_level_doze);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W110, flat_mode);
			mi_disp_panel_update_lhbm_backlight(dsi, TYPE_HLPM_W900, bl_level_doze);
			mi_disp_panel_ddic_send_cmd(lhbm_hlpm_white_110nit, ARRAY_SIZE(lhbm_hlpm_white_110nit), true, true);
		} else {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_WHITE_110, bl_level);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_WHITE_110, flat_mode);
			mi_disp_panel_ddic_send_cmd(lhbm_normal_white_110nit, ARRAY_SIZE(lhbm_normal_white_110nit), true, true);
		}
		ctx->doze_suspend = false;
		break;
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		pr_info("LOCAL_HBM_NORMAL_GREEN_500NIt\n");
		mi_disp_panel_update_lhbm_alpha(dsi, TYPE_GREEN_500, bl_level);
		mi_disp_panel_update_lhbm_white_param(dsi, TYPE_GREEN_500, flat_mode);
		mi_disp_panel_ddic_send_cmd(lhbm_normal_green_500nit, ARRAY_SIZE(lhbm_normal_green_500nit), true, true);
		ctx->doze_suspend = false;
		break;
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
		pr_info("LOCAL_HBM_NORMAL_WHITE_900NIT in HBM\n");
		if (atomic_read(&doze_enable)) {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W900, bl_level_doze);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W900, flat_mode);
			mi_disp_panel_update_lhbm_backlight(dsi, TYPE_HLPM_W900, bl_level_doze);
			mi_disp_panel_ddic_send_cmd(lhbm_hlpm_white_900nit, ARRAY_SIZE(lhbm_hlpm_white_900nit), true, true);
		} else {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_WHITE_900, bl_level);
			mi_disp_panel_update_lhbm_white_param(dsi, TYPE_WHITE_900, flat_mode);
			mi_disp_panel_ddic_send_cmd(lhbm_normal_white_900nit, ARRAY_SIZE(lhbm_normal_white_900nit), false, true);
		}
		ctx->doze_suspend = false;
		break;
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
		pr_info("LOCAL_HBM_NORMAL_WHITE_900NIT in HBM\n");
		mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W900, flat_mode);
		if (atomic_read(&doze_enable) || !bl_level) {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W900, bl_level_doze);
		} else {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W900, bl_level);
		}
		mi_disp_panel_update_lhbm_backlight(dsi, TYPE_HLPM_W900, bl_level_doze);
		mi_disp_panel_ddic_send_cmd(lhbm_hlpm_white_900nit, ARRAY_SIZE(lhbm_hlpm_white_900nit), true, true);
		ctx->doze_suspend = false;
		break;
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		pr_info("LOCAL_HBM_HLPM_WHITE_110NIT\n");
		mi_cfg->dimming_state = STATE_DIM_BLOCK;
		mi_disp_panel_update_lhbm_white_param(dsi, TYPE_HLPM_W110, flat_mode);
		if (atomic_read(&doze_enable) || !bl_level) {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W110, bl_level_doze);
		} else {
			mi_disp_panel_update_lhbm_alpha(dsi, TYPE_HLPM_W110, bl_level);
		}
		mi_disp_panel_update_lhbm_backlight(dsi, TYPE_HLPM_W900, bl_level_doze);
		mi_disp_panel_ddic_send_cmd(lhbm_hlpm_white_110nit, ARRAY_SIZE(lhbm_hlpm_white_110nit), true,true);
		ctx->doze_suspend = false;
		break;
	default:
		pr_info("invalid local hbm value\n");
		break;
	}

	return 0;

}

bool is_hbm_fod_on(struct mtk_dsi *dsi)
{
	struct mi_dsi_panel_cfg *mi_cfg = &dsi->mi_cfg;
	int feature_val;
	bool is_fod_on = false;
	feature_val = mi_cfg->feature_val[DISP_FEATURE_LOCAL_HBM];
	switch (feature_val) {
	case LOCAL_HBM_NORMAL_WHITE_1000NIT:
	case LOCAL_HBM_NORMAL_WHITE_750NIT:
	case LOCAL_HBM_NORMAL_WHITE_500NIT:
	case LOCAL_HBM_NORMAL_WHITE_110NIT:
	case LOCAL_HBM_NORMAL_GREEN_500NIT:
	case LOCAL_HBM_HLPM_WHITE_1000NIT:
	case LOCAL_HBM_HLPM_WHITE_110NIT:
		is_fod_on = true;
		break;
	default:
		break;
	}
	if (mi_cfg->feature_val[DISP_FEATURE_HBM_FOD] == FEATURE_ON) {
		is_fod_on = true;
	}
	return is_fod_on;
}

static int panel_doze_suspend (struct drm_panel *panel, void * dsi, dcs_write_gce cb, void *handle) 
{
	struct lcm *ctx = NULL;
	char esd_off_page1[] = {0xF0,0x55,0xAA,0x52,0x08,0x00};
	char esd_off_page2[] = {0x6F,0x01};
	char esd_off_page3[] = {0xBE,0x41};
	char brightnessh_tb[] = {0x51, 0x00, 0x14,0x00,0x14,0x01,0xFF};
	char cmd1_tb[] = {0x39,0x00};
	char normal2aod_tb[] ={0x65, 0x01};
	char cmd2_tb[] = {0x2C, 0x00};

	if (!dsi) {
		pr_err("dsi is null\n");
		return -1;
	}

	ctx = panel_to_lcm(panel);

	if (!ctx) {
		pr_err("ctx is null\n");
		return -1;
	}

	if (is_hbm_fod_on(dsi)) {
		pr_info("enter doze in doze_suspend skip due to fod on\n");
		return 0;
	}

	if (ctx->doze_suspend) {
		pr_info("%s already suspend, skip\n", __func__);
		goto exit;
	}

	if (ctx->doze_brightness_state == DOZE_BRIGHTNESS_HBM) {
		brightnessh_tb[2] = 0xF6;
		brightnessh_tb[4] = 0xF6;
		brightnessh_tb[5] = 0x03;
	}
	cb(dsi, handle, esd_off_page1, ARRAY_SIZE(esd_off_page1));
	cb(dsi, handle, esd_off_page2, ARRAY_SIZE(esd_off_page2));
	cb(dsi, handle, esd_off_page3, ARRAY_SIZE(esd_off_page3));
	cb(dsi, handle, brightnessh_tb, ARRAY_SIZE(brightnessh_tb));
	cb(dsi, handle, cmd1_tb, ARRAY_SIZE(cmd1_tb));
	cb(dsi, handle, normal2aod_tb, ARRAY_SIZE(normal2aod_tb));
	cb(dsi, handle, cmd2_tb, ARRAY_SIZE(cmd2_tb));
	ctx->doze_suspend = true;
	usleep_range(5 * 1000, 5* 1000 + 10);
	pr_info("lhbm enter aod in doze_suspend\n");

exit:
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
	dsi->mi_cfg.lhbm_ui_ready_delay_frame = 5;
	dsi->mi_cfg.lhbm_ui_ready_delay_frame_aod = 7;
	dsi->mi_cfg.local_hbm_enabled = 1;
	dsi->mi_cfg.fod_low_brightness_allow = 1;
	dsi->mi_cfg.fod_low_brightness_clone_threshold = 200;
	dsi->mi_cfg.fod_low_brightness_lux_threshold = 3;

	return 0;
}

static struct mtk_panel_funcs ext_funcs = {
	.reset = panel_ext_reset,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.mode_switch = mode_switch,
	.panel_poweroff = lcm_panel_poweroff,
	.doze_enable = panel_doze_enable,
	.doze_disable = panel_doze_disable,
	.setbacklight_control = lcm_setbacklight_control,
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
	.hbm_fod_control = panel_hbm_fod_control,
	.get_wp_info = panel_get_wp_info,
	.get_grayscale_info = panel_get_grayscale_info,
	.set_spr_status = panel_set_spr_status,
	.panel_set_dc_crc = panel_set_dc_crc,
	.panel_set_dc_crc_off = panel_set_dc_crc_off,
	.set_dc_backlight = panel_set_dc_backlight,
	.set_dc_threshold = panel_set_dc_threshold,
	.set_lhbm_fod = panel_set_lhbm_fod,
	.panel_fod_lhbm_init = panel_fod_lhbm_init,
	.doze_suspend = panel_doze_suspend,
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
	struct drm_display_mode *mode0, *mode1, *mode2;

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

	pr_info("l12a_36_02_0b_dsc_cmd %s-\n", __func__);

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
	{ .compatible = "l12a_36_02_0b_dsc_cmd,lcm", },
	{ }
};

MODULE_DEVICE_TABLE(of, lcm_of_match);

static struct mipi_dsi_driver lcm_driver = {
	.probe = lcm_probe,
	.remove = lcm_remove,
	.driver = {
		.name = "l12a_36_02_0b_dsc_cmd,lcm",
		.owner = THIS_MODULE,
		.of_match_table = lcm_of_match,
	},
};

module_mipi_dsi_driver(lcm_driver);

module_param_string(oled_wp, oled_wp_cmdline, sizeof(oled_wp_cmdline), 0600);
MODULE_PARM_DESC(oled_wp, "oled_wp=<white_point_info>");

module_param_string(oled_lhbm, oled_lhbm_cmdline, sizeof(oled_lhbm_cmdline), 0600);
MODULE_PARM_DESC(oled_lhbm, "oled_lhbm=<local_hbm_info>");

module_param_string(oled_grayscale, oled_grayscale_cmdline, sizeof(oled_grayscale_cmdline), 0600);
MODULE_PARM_DESC(oled_grayscale, "oled_grayscale=<grayscale_info>");

MODULE_AUTHOR("Xiaobo Xie <xiexiaobo@xiaomi.com>");
MODULE_DESCRIPTION("L12a 36 02 0b dsc wqhd oled panel driver");
MODULE_LICENSE("GPL v2");
