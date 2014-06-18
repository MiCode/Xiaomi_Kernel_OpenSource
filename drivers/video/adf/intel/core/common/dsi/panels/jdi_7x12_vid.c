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

#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_mid_remoteproc.h>
#include <linux/gpio.h>

#include "intel_adf_device.h"
#include "core/common/dsi/dsi_panel.h"
#include "core/common/dsi/dsi_pkg_sender.h"
#include "core/common/dsi/dsi_config.h"
#include "core/common/dsi/dsi_pipe.h"

/*JDI panel info*/
#define JDI_VID_PHY_WIDTH	56
#define JDI_VID_PHY_HEIGHT	99
#define JDI_VID_BPP		24
#define JDI_VID_DSI_TYPE	DSI_DPI
#define JDI_VID_LANE_NUM	3

/* The register to control secure I2C FLIS pin */
#define SECURE_I2C_FLIS_REG	0xFF0C1D30

static int mipi_reset_gpio;
static int bias_en_gpio;

static u8 jdi_set_address_mode[] = {0x36, 0xc0, 0x00, 0x00};
static u8 jdi_write_display_brightness[] = {0x51, 0x0f, 0xff, 0x00};

static int jdi_vid_drv_ic_init(struct dsi_pipe *pipe)
{
	struct dsi_pkg_sender *sender = &pipe->sender;
	int err = 0;

	pr_debug("\n");

	if (!sender) {
		pr_err("Cannot get sender\n");
		return -EINVAL;
	}

	/* Set Address Mode */
	err = dsi_send_mcs_long_hs(sender, jdi_set_address_mode,
			4,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Address Mode\n", __func__, __LINE__);
		goto ic_init_err;
	}

	/* Set Pixel format */
	err = dsi_send_mcs_short_hs(sender, set_pixel_format, 0x70, 1,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Pixel format\n", __func__, __LINE__);
		goto ic_init_err;
	}

	/* change "ff0f" according to the brightness desired. */
	err = dsi_send_mcs_long_hs(sender, jdi_write_display_brightness,
			4, DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Brightness\n", __func__, __LINE__);
		goto ic_init_err;
	}

	/* Write control display */
	err = dsi_send_mcs_short_hs(sender, write_ctrl_display, 0x24, 1,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Write Control Display\n", __func__,
				__LINE__);
		goto ic_init_err;
	}

	/* Write control CABC */
	err = dsi_send_mcs_short_hs(sender, write_ctrl_cabc, STILL_IMAGE,
			1, DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Write Control CABC\n", __func__, __LINE__);
		goto ic_init_err;
	}

	return 0;

ic_init_err:
	err = -EIO;
	return err;
}

static void jdi_vid_controller_init(struct dsi_pipe *pipe)
{
	struct dsi_context *hw_ctx = &pipe->config.ctx;

	pr_debug("\n");

	/*reconfig lane configuration*/
	pipe->config.lane_count = 3;
	pipe->config.lane_config = DSI_DATA_LANE_4_0;
	hw_ctx->pll_bypass_mode = 0;
	/* This is for 400 mhz.  Set it to 0 for 800mhz */
	hw_ctx->cck_div = 1;

	hw_ctx->mipi_control = READ_REQUEST_HIGH_PRIORITY;
	hw_ctx->intr_en = 0xFFFFFFFF;
	hw_ctx->hs_tx_timeout = 0xFFFFFF;
	hw_ctx->lp_rx_timeout = 0xFFFFFF;
	hw_ctx->turn_around_timeout = 0xFFFF;
	hw_ctx->device_reset_timer = 0xFF;
	hw_ctx->high_low_switch_count = 0x20;
	hw_ctx->clk_lane_switch_time_cnt = 0x0020000E;
	hw_ctx->dbi_bw_ctrl = 0x0;
	hw_ctx->eot_disable = 0x0;
	hw_ctx->init_count = 0x7D0;
	hw_ctx->lp_byteclk = 0x4;
	hw_ctx->dphy_param = 0x1B0F4115;

	/*setup video mode format*/
	hw_ctx->video_mode_format = BURST_MODE | DISABLE_VIDEO_BTA;

	/*set up func_prg*/
	hw_ctx->dsi_func_prg = (MIPI_FMT_RGB888 | VID_VIRTUAL_CHANNEL_0 |
			pipe->config.lane_count);

	/*setup mipi port configuration*/
	hw_ctx->mipi = MIPI_PORT_ENABLE | LP_OUTPUT_HOLD_ENABLE |
		BANDGAP_CHICKEN_BIT | pipe->config.lane_config;
}

static int jdi_vid_panel_connection_detect(struct dsi_pipe *pipe)
{
	int status;
	int idx = pipe->base.base.idx;

	pr_debug("\n");

	if (idx == 0)
		status = DSI_PANEL_CONNECTED;
	else {
		pr_info("%s: do NOT support dual panel\n", __func__);
		status = DSI_PANEL_DISCONNECTED;
	}

	return status;
}

static int jdi_vid_power_on(struct dsi_pipe *pipe)
{
	struct dsi_pkg_sender *sender = &pipe->sender;
	int err;

	pr_debug("\n");

	if (!sender) {
		pr_err("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	/* Sleep Out */
	err = dsi_send_mcs_short_hs(sender, exit_sleep_mode, 0, 0,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Exit Sleep Mode\n", __func__, __LINE__);
		goto power_on_err;
	}
	/* Wait for 6 frames after exit_sleep_mode. */
	msleep(100);

	/* Set Display on */
	err = dsi_send_mcs_short_hs(sender, set_display_on, 0, 0,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Display On\n", __func__, __LINE__);
		goto power_on_err;
	}
	/* Wait for 1 frame after set_display_on. */
	msleep(20);

	/* Send TURN_ON packet */
	err = dsi_send_dpi_spk_pkg_hs(sender, DSI_DPI_SPK_TURN_ON);
	if (err) {
		pr_err("Failed to send turn on packet\n");
		goto power_on_err;
	}

	return 0;

power_on_err:
	err = -EIO;
	return err;
}

static void __vpro2_power_ctrl(bool on)
{
	u8 addr, value;
	addr = 0xad;
	if (intel_scu_ipc_ioread8(addr, &value))
		pr_err("%s: %d: failed to read vPro2\n", __func__, __LINE__);

	/* Control vPROG2 power rail with 2.85v. */
	if (on)
		value |= 0x1;
	else
		value &= ~0x1;

	if (intel_scu_ipc_iowrite8(addr, value))
		pr_err("%s: %d: failed to write vPro2\n", __func__, __LINE__);
}

static int jdi_vid_power_off(struct dsi_pipe *pipe)
{
	struct dsi_pkg_sender *sender = &pipe->sender;
	int err;

	pr_debug("\n");

	if (!sender) {
		pr_err("Failed to get DSI packet sender\n");
		return -EINVAL;
}

	/*send SHUT_DOWN packet */
	err = dsi_send_dpi_spk_pkg_hs(sender,
			DSI_DPI_SPK_SHUT_DOWN);
	if (err) {
		pr_err("Failed to send turn off packet\n");
		goto power_off_err;
	}
	/* According HW DSI spec, need to wait for 100ms. */
	msleep(100);

	/* Set Display off */
	err = dsi_send_mcs_short_hs(sender, set_display_off, 0, 0,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Set Display On\n", __func__, __LINE__);
		goto power_off_err;
	}
	/* Wait for 1 frame after set_display_on. */
	msleep(20);

	/* Sleep In */
	err = dsi_send_mcs_short_hs(sender, enter_sleep_mode, 0, 0,
			DSI_SEND_PACKAGE);
	if (err) {
		pr_err("%s: %d: Exit Sleep Mode\n", __func__, __LINE__);
		goto power_off_err;
	}
	/* Wait for 3 frames after enter_sleep_mode. */
	msleep(51);

	/* Can not poweroff VPROG2, because many other module related to
	 * this power supply, such as PSH sensor. */
	/*__vpro2_power_ctrl(false);*/
	if (bias_en_gpio)
		gpio_set_value_cansleep(bias_en_gpio, 0);

	return 0;

power_off_err:
	err = -EIO;
	return err;
}

static int jdi_vid_set_brightness(struct dsi_pipe *pipe, int level)
{
	struct dsi_pkg_sender *sender = &pipe->sender;
	int duty_val = 0;

	pr_debug("level = %d\n", level);

	if (!sender) {
		pr_err("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	duty_val = (0xFFF * level) / 100;

	/*
	 * Note: the parameters of write_display_brightness in JDI R69001 spec
	 * map DBV[7:4] as MSB.
	 */
	jdi_write_display_brightness[0] = 0x51;
	jdi_write_display_brightness[1] = duty_val & 0xF;
	jdi_write_display_brightness[2] = ((duty_val & 0xFF0) >> 4);
	jdi_write_display_brightness[3] = 0x0;
	dsi_send_mcs_long_hs(sender, jdi_write_display_brightness, 4, 0);

	return 0;
}

static int jdi_vid_panel_reset(struct dsi_pipe *pipe)
{
	u8 *vaddr1 = NULL;
	int reg_value_scl = 0;
	int ret = 0;

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

	/* For meeting tRW1 panel spec */
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
	if (mipi_reset_gpio == 0) {
		ret = get_gpio_by_name("disp0_rst");
		if (ret < 0) {
			pr_err("Faild to get panel reset gpio, use default reset pin\n");
			return -EINVAL;
		}
		mipi_reset_gpio = ret;
		ret = gpio_request(mipi_reset_gpio, "mipi_display");
		if (ret) {
			pr_err("Faild to request panel reset gpio\n");
			return -EINVAL;
		}
		gpio_direction_output(mipi_reset_gpio, 0);
	}
	gpio_direction_output(bias_en_gpio, 0);
	gpio_direction_output(mipi_reset_gpio, 0);
	gpio_set_value_cansleep(bias_en_gpio, 0);
	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	usleep_range(2000, 2500);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	usleep_range(2000, 2500);
	gpio_set_value_cansleep(bias_en_gpio, 1);
	usleep_range(2000, 2500);
	/* switch i2c scl pin back */
	reg_value_scl |= 0x1000;
	rpmsg_send_generic_raw_command(RP_INDIRECT_WRITE, 0,
			(u8 *)&reg_value_scl, 4,
			NULL, 0,
			SECURE_I2C_FLIS_REG, 0);
	iounmap(vaddr1);

	return 0;
}

static int jdi_vid_get_config_mode(struct drm_mode_modeinfo *mode)
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
	mode->clock = mode->vrefresh * mode->vtotal * mode->htotal / 1000;

	adf_modeinfo_set_name(mode);

	return 0;
}

static void jdi_vid_get_panel_info(struct panel_info *pi)
{
	pr_debug("\n");

	pi->width_mm = JDI_VID_PHY_WIDTH;
	pi->height_mm = JDI_VID_PHY_HEIGHT;
	pi->bpp = JDI_VID_BPP;
	pi->dsi_type = JDI_VID_DSI_TYPE;
	pi->lane_num = JDI_VID_LANE_NUM;
	pi->dual_link = DSI_PANEL_SINGLE_LINK;
}

static struct panel_ops jdi_vid_panel_ops = {
	.drv_ic_init = jdi_vid_drv_ic_init,
	.dsi_controller_init = jdi_vid_controller_init,
	.detect = jdi_vid_panel_connection_detect,
	.power_on = jdi_vid_power_on,
	.power_off = jdi_vid_power_off,
	.set_brightness = jdi_vid_set_brightness,
	.reset = jdi_vid_panel_reset,
	.get_config_mode = jdi_vid_get_config_mode,
	.exit_deep_standby = NULL,
	.get_panel_info = jdi_vid_get_panel_info,
};

struct dsi_panel jdi_vid_panel = {
	.panel_id = JDI_7x12_VID,
	.info.width_mm = JDI_VID_PHY_WIDTH,
	.info.height_mm = JDI_VID_PHY_HEIGHT,
	.info.bpp = JDI_VID_BPP,
	.info.dsi_type = JDI_VID_DSI_TYPE,
	.info.lane_num = JDI_VID_LANE_NUM,
	.info.dual_link = DSI_PANEL_SINGLE_LINK,
	.ops = &jdi_vid_panel_ops,
};

const struct dsi_panel *jdi_vid_get_panel(void)
{
	return &jdi_vid_panel;
}
