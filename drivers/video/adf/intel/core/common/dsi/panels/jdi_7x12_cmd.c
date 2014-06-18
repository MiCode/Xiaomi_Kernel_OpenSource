/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/

#include <linux/gpio.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_mid_remoteproc.h>

#include "intel_adf_device.h"
#include "core/common/dsi/dsi_panel.h"
#include "core/common/dsi/dsi_pkg_sender.h"
#include "core/common/dsi/dsi_config.h"
#include "core/common/dsi/dsi_pipe.h"

#include "pwr_mgmt.h"

/*JDI panel info*/
#define JDI_CMD_PHY_WIDTH	56
#define JDI_CMD_PHY_HEIGHT	99
#define JDI_CMD_BPP		24
#define JDI_CMD_DSI_TYPE	DSI_DBI
#define JDI_CMD_LANE_NUM	3

/* The register to control secure I2C FLIS pin */
#define SECURE_I2C_FLIS_REG	0xFF0C1D30

static int mipi_reset_gpio;
static int bias_en_gpio;

static u8 jdi_mcs_clumn_addr[] = {0x2a, 0x00, 0x00, 0x02, 0xcf};
static u8 jdi_mcs_page_addr[] = {
			0x2b, 0x00, 0x00, 0x04, 0xff};
static u8 jdi_timing_control[] = {
			0xc6, 0x6d, 0x05, 0x60, 0x05,
			0x60, 0x01, 0x01, 0x01, 0x02,
			0x01, 0x02, 0x01, 0x01, 0x01,
			0x01, 0x01, 0x01, 0x05, 0x15,
			0x09
};

static int jdi_cmd_drv_ic_init(struct dsi_pipe *pipe)
{
	struct dsi_pkg_sender *sender = &pipe->sender;
	int err = 0;

	pr_debug("\n");

	if (!sender) {
		pr_err("Cannot get sender\n");
		return -EINVAL;
	}
	err = dsi_send_mcs_short_hs(sender,
			exit_sleep_mode, 0, 0,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Exit Sleep Mode\n",
		__func__, __LINE__);
		goto ic_init_err;
	}

	msleep(120);
	err = dsi_send_mcs_short_hs(sender,
			write_display_brightness, 0x0, 1,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Brightness\n",
		__func__, __LINE__);
		goto ic_init_err;
	}

	err = dsi_send_mcs_short_hs(sender,
			write_ctrl_display, 0x24, 1,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Write Control Display\n",
		__func__, __LINE__);
		goto ic_init_err;
	}

	err = dsi_send_mcs_short_hs(sender,
			write_ctrl_cabc, STILL_IMAGE, 1,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Write Control CABC\n",
		__func__, __LINE__);
		goto ic_init_err;
	}

	if (!IS_ANN()) {
		err = dsi_send_mcs_short_hs(sender,
				write_cabc_min_bright, 51, 1,
				DSI_SEND_PACKAGE);
		if (err) {
			pr_err("%s: %d: Write CABC minimum brightness\n",
					__func__, __LINE__);
			goto ic_init_err;
		}
		err = dsi_send_gen_short_hs(sender,
				access_protect, 4, 2,
				DSI_SEND_PACKAGE);
		if (err) {
			pr_err("%s: %d: Manufacture command protect on\n",
					__func__, __LINE__);
			goto ic_init_err;
		}

		err = dsi_send_gen_long_lp(sender,
				jdi_timing_control,
				21, DSI_SEND_PACKAGE);
		if (err) {
			pr_err("%s: %d: Set panel timing\n",
					__func__, __LINE__);
			goto ic_init_err;
		}
		msleep(20);
	}

	err = dsi_send_gen_short_hs(sender,
			access_protect, 4, 2,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Manufacture command protect on\n",
			__func__, __LINE__);
		goto ic_init_err;
	}

	err = dsi_send_gen_long_lp(sender,
			jdi_timing_control,
			21, DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set panel timing\n",
				__func__, __LINE__);
		goto ic_init_err;
	}
	msleep(20);

	err = dsi_send_mcs_short_hs(sender,
			set_tear_on, 0x00, 1,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Tear On\n",
		__func__, __LINE__);
		goto ic_init_err;
	}

	err = dsi_send_mcs_long_hs(sender,
			jdi_mcs_clumn_addr,
			5, DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Clumn Address\n",
		__func__, __LINE__);
		goto ic_init_err;
	}

	err = dsi_send_mcs_long_hs(sender,
			jdi_mcs_page_addr,
			5, DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Page Address\n",
		__func__, __LINE__);
		goto ic_init_err;
	}

	return 0;

ic_init_err:
	err = -EIO;
	return err;
}

static void jdi_cmd_controller_init(struct dsi_pipe *pipe)
{

	struct dsi_context *hw_ctx = &pipe->config.ctx;

#ifdef ENABLE_CSC_GAMMA /*FIXME*/
	struct csc_setting csc = {
			.pipe = 0,
			.type = CSC_REG_SETTING,
			.enable_state = true,
			.data_len = CSC_REG_COUNT,
			.data.csc_reg_data = {
			0xFFB0424, 0xFDF, 0x4320FF1,
			0xFDC, 0xFF50FF5, 0x415}
		 };
	struct gamma_setting gamma = {
		.pipe = 0,
		.type = GAMMA_REG_SETTING,
		.enable_state = true,
		.data_len = GAMMA_10_BIT_TABLE_COUNT,
		.gamma_tableX100 = {
			0x000000, 0x030303, 0x050505, 0x070707,
			0x090909, 0x0C0C0C, 0x0E0E0E, 0x101010,
			0x121212, 0x141414, 0x171717, 0x191919,
			0x1B1B1B, 0x1D1D1D, 0x1F1F1F, 0x212121,
			0x232323, 0x252525, 0x282828, 0x2A2A2A,
			0x2C2C2C, 0x2E2E2E, 0x303030, 0x323232,
			0x343434, 0x363636, 0x383838, 0x3A3A3A,
			0x3C3C3C, 0x3E3E3E, 0x404040, 0x424242,
			0x444444, 0x464646, 0x484848, 0x4A4A4A,
			0x4C4C4C, 0x4E4E4E, 0x505050, 0x525252,
			0x545454, 0x565656, 0x585858, 0x5A5A5A,
			0x5C5C5C, 0x5E5E5E, 0x606060, 0x626262,
			0x646464, 0x666666, 0x686868, 0x6A6A6A,
			0x6C6C6C, 0x6E6E6E, 0x707070, 0x727272,
			0x747474, 0x767676, 0x787878, 0x7A7A7A,
			0x7C7C7C, 0x7E7E7E, 0x808080, 0x828282,
			0x848484, 0x868686, 0x888888, 0x8A8A8A,
			0x8C8C8C, 0x8E8E8E, 0x909090, 0x929292,
			0x949494, 0x969696, 0x989898, 0x999999,
			0x9B9B9B, 0x9D9D9D, 0x9F9F9F, 0xA1A1A1,
			0xA3A3A3, 0xA5A5A5, 0xA7A7A7, 0xA9A9A9,
			0xABABAB, 0xADADAD, 0xAFAFAF, 0xB1B1B1,
			0xB3B3B3, 0xB5B5B5, 0xB6B6B6, 0xB8B8B8,
			0xBABABA, 0xBCBCBC, 0xBEBEBE, 0xC0C0C0,
			0xC2C2C2, 0xC4C4C4, 0xC6C6C6, 0xC8C8C8,
			0xCACACA, 0xCCCCCC, 0xCECECE, 0xCFCFCF,
			0xD1D1D1, 0xD3D3D3, 0xD5D5D5, 0xD7D7D7,
			0xD9D9D9, 0xDBDBDB, 0xDDDDDD, 0xDFDFDF,
			0xE1E1E1, 0xE3E3E3, 0xE4E4E4, 0xE6E6E6,
			0xE8E8E8, 0xEAEAEA, 0xECECEC, 0xEEEEEE,
			0xF0F0F0, 0xF2F2F2, 0xF4F4F4, 0xF6F6F6,
			0xF7F7F7, 0xF9F9F9, 0xFBFBFB, 0xFDFDFD}
	 };
#endif

	pr_debug("\n");

	/*reconfig lane configuration*/
	pipe->config.lane_count = 3;
	pipe->config.lane_config = DSI_DATA_LANE_4_0;
	/* FIXME: enable CSC and GAMMA */
	/*dsi_config->enable_gamma_csc = ENABLE_GAMMA | ENABLE_CSC;*/
	/* This is for 400 mhz.  Set it to 0 for 800mhz */
	hw_ctx->cck_div = 1;
	hw_ctx->pll_bypass_mode = 0;

	if (IS_ANN()) {
		hw_ctx->mipi_control = 0x18;
		hw_ctx->intr_en = 0xFFFFFFFF;
		hw_ctx->hs_tx_timeout = 0xFFFFFF;
		hw_ctx->lp_rx_timeout = 0xFFFFFF;
		hw_ctx->device_reset_timer = 0xff;
		hw_ctx->turn_around_timeout = 0xffff;
		hw_ctx->high_low_switch_count = 0x20;
		hw_ctx->clk_lane_switch_time_cnt = 0x21000e;
		hw_ctx->lp_byteclk = 0x4;
		hw_ctx->dphy_param = 0x1b104315;
		hw_ctx->eot_disable = 0x1;
		hw_ctx->init_count = 0x7d0;
		hw_ctx->dbi_bw_ctrl = 1390;
		hw_ctx->hs_ls_dbi_enable = 0x0;
		hw_ctx->dsi_func_prg = ((DBI_DATA_WIDTH_OPT2 << 13) |
				pipe->config.lane_count);
		hw_ctx->mipi = SEL_FLOPPED_HSTX	| PASS_FROM_SPHY_TO_AFE |
			BANDGAP_CHICKEN_BIT | TE_TRIGGER_GPIO_PIN;
	} else {
		hw_ctx->mipi_control = 0x0;
		hw_ctx->intr_en = 0xFFFFFFFF;
		hw_ctx->hs_tx_timeout = 0xFFFFFF;
		hw_ctx->lp_rx_timeout = 0xFFFFFF;
		hw_ctx->device_reset_timer = 0xffff;
		hw_ctx->turn_around_timeout = 0x1a;
		hw_ctx->high_low_switch_count = 0x21;
		hw_ctx->clk_lane_switch_time_cnt = 0x21000f;
		hw_ctx->lp_byteclk = 0x5;
		hw_ctx->dphy_param = 0x25155b1e;
		hw_ctx->eot_disable = 0x3;
		hw_ctx->init_count = 0xf0;
		hw_ctx->dbi_bw_ctrl = 1390;
		hw_ctx->hs_ls_dbi_enable = 0x0;
		hw_ctx->dsi_func_prg = ((DBI_DATA_WIDTH_OPT2 << 13) |
				pipe->config.lane_count);
		hw_ctx->mipi = PASS_FROM_SPHY_TO_AFE |
			BANDGAP_CHICKEN_BIT |
			TE_TRIGGER_GPIO_PIN;
	}
	hw_ctx->video_mode_format = 0xf;

#ifdef ENABLE_CSC_GAMMA /*FIXME*/
	if (pipe->config.enable_gamma_csc & ENABLE_CSC) {
		/* setting the tuned csc setting */
		drm_psb_enable_color_conversion = 1;
		intel_crtc_set_color_conversion(dev, &csc);
	}

	if (pipe->config.enable_gamma_csc & ENABLE_GAMMA) {
		/* setting the tuned gamma setting */
		drm_psb_enable_gamma = 1;
		intel_crtc_set_gamma(dev, &gamma);
	}
#endif

}
static int jdi_cmd_panel_connection_detect(struct dsi_pipe *pipe)
{
	int status;
	int idx = pipe->base.base.idx;

	pr_debug("\n");

	if (idx == 0) {
		status = DSI_PANEL_CONNECTED;
	} else {
		pr_info("%s: do NOT support dual panel\n",
		__func__);
		status = DSI_PANEL_DISCONNECTED;
	}

	return status;
}

static int jdi_cmd_power_on(struct dsi_pipe *pipe)
{

	struct dsi_pkg_sender *sender = &pipe->sender;
	int err = 0;

	pr_debug("\n");

	if (!sender) {
		pr_err("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	err = dsi_send_mcs_short_hs(sender,
			set_address_mode, 0x0, 1,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Address Mode\n",
		__func__, __LINE__);
		goto power_err;
	}
	usleep_range(20000, 20100);

	err = dsi_send_mcs_short_hs(sender,
			set_pixel_format, 0x77, 1,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Pixel format\n",
		__func__, __LINE__);
		goto power_err;
	}

	/*turn on display*/
	err = dsi_send_dcs(sender,
		 set_display_on,
		 NULL,
		 0,
		 CMD_DATA_SRC_SYSTEM_MEM,
		 DSI_SEND_PACKAGE);
	if (err) {
		pr_err("faild to set_display_on mode\n");
		goto power_err;
	}
	usleep_range(20000, 20100);

power_err:
	return err;
}

static void __vpro2_power_ctrl(bool on)
{
	u8 addr, value;
	addr = 0xad;
	if (intel_scu_ipc_ioread8(addr, &value))
		pr_err("%s: %d: failed to read vPro2\n",
		__func__, __LINE__);

	/* Control vPROG2 power rail with 2.85v. */
	if (on)
		value |= 0x1;
	else
		value &= ~0x1;

	if (intel_scu_ipc_iowrite8(addr, value))
		pr_err("%s: %d: failed to write vPro2\n",
				__func__, __LINE__);
}

static int jdi_cmd_power_off(struct dsi_pipe *pipe)
{
	struct dsi_pkg_sender *sender = &pipe->sender;
	int err;

	pr_debug("\n");

	if (!sender) {
		pr_err("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	err = dsi_send_mcs_short_hs(sender,
			set_display_off, 0, 0,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Display Off\n",
		__func__, __LINE__);
		goto power_off_err;
	}
	usleep_range(20000, 20100);

	err = dsi_send_mcs_short_hs(sender,
			set_tear_off, 0, 0,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Tear Off\n",
		__func__, __LINE__);
		goto power_off_err;
	}

	err = dsi_send_mcs_short_hs(sender,
			enter_sleep_mode, 0, 0,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Enter Sleep Mode\n",
		__func__, __LINE__);
		goto power_off_err;
	}

	msleep(60);

	err = dsi_send_gen_short_hs(sender,
		access_protect, 4, 2,
		DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Access Protect\n",
		__func__, __LINE__);
		goto power_off_err;
	}

	err = dsi_send_gen_short_hs(sender,
		low_power_mode, 1, 2,
		DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Low Power Mode\n",
		__func__, __LINE__);
		goto power_off_err;
	}
	if (bias_en_gpio)
		gpio_set_value_cansleep(bias_en_gpio, 0);
	usleep_range(1000, 1500);
	return 0;
power_off_err:
	err = -EIO;
	return err;
}

static int jdi_cmd_set_brightness(struct dsi_pipe *pipe,
				int level)
{
	struct dsi_pkg_sender *sender = &pipe->sender;
	u8 duty_val = 0;

	pr_debug("level = %d\n", level);

	if (!sender) {
		pr_err("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	duty_val = (0xFF * level) / 100;
	dsi_send_mcs_short_hs(sender,
			write_display_brightness, duty_val, 1,
			DSI_SEND_PACKAGE);
	return 0;
}

static void _get_panel_reset_gpio(void)
{
	int ret = 0;
	if (mipi_reset_gpio == 0) {
		ret = get_gpio_by_name("disp0_rst");
		if (ret < 0) {
			pr_err("Faild to get panel reset gpio, use default reset pin\n");
			return;
		}
		mipi_reset_gpio = ret;
		ret = gpio_request(mipi_reset_gpio, "mipi_display");
		if (ret) {
			pr_err("Faild to request panel reset gpio\n");
			return;
		}
		gpio_direction_output(mipi_reset_gpio, 0);
	}
}

static int jdi_cmd_panel_reset(struct dsi_pipe *pipe)
{
	int ret = 0;
	u8 *vaddr = NULL, *vaddr1 = NULL;
	int reg_value_scl = 0;

	pr_debug("\n");

	/* Because when reset touchscreen panel, touchscreen will pull i2c bus
	 * to low, sometime this operation will cause i2c bus enter into wrong
	 * status, so before reset, switch i2c scl pin */
	vaddr1 = ioremap(SECURE_I2C_FLIS_REG, 4);
	reg_value_scl = ioread32(vaddr1);
	reg_value_scl &= ~0x1000;
	rpmsg_send_generic_raw_command(RP_INDIRECT_WRITE, 0,
					(u8 *)&reg_value_scl, 4,
					NULL, 0,
					SECURE_I2C_FLIS_REG, 0);

	__vpro2_power_ctrl(true);
	usleep_range(2000, 2500);

	if (bias_en_gpio == 0) {
		bias_en_gpio = 189;
		ret = gpio_request(bias_en_gpio, "bias_enable");
		if (ret) {
			pr_err("Faild to request bias_enable gpio\n");
			return -EINVAL;
		}
		gpio_direction_output(bias_en_gpio, 0);
	}

	_get_panel_reset_gpio();

	gpio_direction_output(bias_en_gpio, 0);
	gpio_direction_output(mipi_reset_gpio, 0);
	gpio_set_value_cansleep(bias_en_gpio, 0);
	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	usleep_range(2000, 2500);
	gpio_set_value_cansleep(bias_en_gpio, 1);
	usleep_range(2000, 2500);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	usleep_range(3000, 3500);
	vaddr = ioremap(0xff0c2d00, 0x60);
	iowrite32(0x3221, vaddr + 0x1c);
	usleep_range(2000, 2500);
	iounmap(vaddr);

	/* switch i2c scl pin back */
	reg_value_scl |= 0x1000;
	rpmsg_send_generic_raw_command(RP_INDIRECT_WRITE, 0,
					(u8 *)&reg_value_scl, 4,
					NULL, 0,
					SECURE_I2C_FLIS_REG, 0);
	iounmap(vaddr1);
	return 0;
}

static int jdi_cmd_exit_deep_standby(struct dsi_pipe *pipe)
{
	pr_debug("\n");

	if (bias_en_gpio)
		gpio_set_value_cansleep(bias_en_gpio, 1);
	_get_panel_reset_gpio();
	gpio_direction_output(mipi_reset_gpio, 0);

	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	usleep_range(1000, 1500);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	usleep_range(3000, 3500);
	return 0;
}

static int jdi_cmd_get_config_mode(struct drm_mode_modeinfo *mode)
{
	pr_debug("\n");

	if (!mode)
		return -EINVAL;

	mode->hdisplay = 720;
	mode->hsync_start = 816;
	mode->hsync_end = 818;
	mode->htotal = 920;

	mode->vdisplay = 1280;
	mode->vsync_start = 1288;
	mode->vsync_end = 1296;
	mode->vtotal = 1304;

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal * mode->htotal / 1000;

	adf_modeinfo_set_name(mode);

	return 0;
}

static void jdi_cmd_get_panel_info(struct panel_info *pi)
{
	pr_debug("\n");

	pi->width_mm = JDI_CMD_PHY_WIDTH;
	pi->height_mm = JDI_CMD_PHY_HEIGHT;
	pi->bpp = JDI_CMD_BPP;
	pi->dsi_type = JDI_CMD_DSI_TYPE;
	pi->lane_num = JDI_CMD_LANE_NUM;
	pi->dual_link = DSI_PANEL_SINGLE_LINK;
}

static struct panel_ops jdi_cmd_panel_ops = {
	.drv_ic_init = jdi_cmd_drv_ic_init,
	.dsi_controller_init = jdi_cmd_controller_init,
	.detect = jdi_cmd_panel_connection_detect,
	.power_on = jdi_cmd_power_on,
	.power_off = jdi_cmd_power_off,
	.set_brightness = jdi_cmd_set_brightness,
	.reset = jdi_cmd_panel_reset,
	.get_config_mode = jdi_cmd_get_config_mode,
	.exit_deep_standby = jdi_cmd_exit_deep_standby,
	.get_panel_info = jdi_cmd_get_panel_info,
};

struct dsi_panel jdi_cmd_panel = {
	.panel_id = JDI_7x12_CMD,
	.info.width_mm = JDI_CMD_PHY_WIDTH,
	.info.height_mm = JDI_CMD_PHY_HEIGHT,
	.info.bpp = JDI_CMD_BPP,
	.info.dsi_type = JDI_CMD_DSI_TYPE,
	.info.lane_num = JDI_CMD_LANE_NUM,
	.info.dual_link = DSI_PANEL_SINGLE_LINK,
	.ops = &jdi_cmd_panel_ops,
};

const struct dsi_panel *jdi_cmd_get_panel(void)
{
	return &jdi_cmd_panel;
}
