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

#define SHARP_10x19_CMD_PHY_WIDTH	58
#define SHARP_10x19_CMD_PHY_HEIGHT	103
#define SHARP_10x19_CMD_BPP		24
#define SHARP_10x19_CMD_DSI_TYPE	DSI_DBI
#define SHARP_10x19_CMD_LANE_NUM	4

/* The register to control secure I2C FLIS pin */
#define SECURE_I2C_FLIS_REG	0xFF0C1D30

#define EXPANDER_BUS_NUMBER 7

static int mipi_reset_gpio;
static int mipic_reset_gpio;
static int bias_en_gpio;

#define sharp10x19_remove_nvm_reload 0xd6
static u8 sharp10x19_mcs_column_addr[] = { 0x2a, 0x00, 0x00, 0x04, 0x37 };
static u8 sharp10x19_mcs_page_addr[] = { 0x2b, 0x00, 0x00, 0x07, 0x7f };

static int sharp10x19_cmd_drv_ic_init(struct dsi_pipe *pipe)
{
	int ret;
	u8 cmd;
	int i;
	int loop = 1;
	struct dsi_pkg_sender *sender = &pipe->sender;
	struct dsi_config *config = &pipe->config;

	if (!sender) {
		pr_err("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	if (is_dual_panel(config))
		loop = 2;

	for (i = 0; i < loop; i++) {
		if (i == 0)
			sender->work_for_slave_panel = false;
		else
			sender->work_for_slave_panel = true;

		/* exit sleep */
		cmd = exit_sleep_mode;
		ret = dsi_send_mcs_short_hs(sender,
				cmd, 0, 0, DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;
		msleep(120);

		/* unlock MCW */
		cmd = access_protect;
		ret = dsi_send_mcs_short_hs(sender,
				cmd, 0x0, 1, DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* reload NVM */
		cmd = sharp10x19_remove_nvm_reload;
		ret = dsi_send_mcs_short_hs(sender,
				cmd, 0x1, 1, DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* send display brightness */
		cmd = write_display_brightness;
		ret = dsi_send_mcs_short_hs(sender,
				cmd, 0xff, 1, DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* display control */
		cmd = write_ctrl_display;
		ret = dsi_send_mcs_short_hs(sender,
				cmd, 0x0c, 1, DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* CABC */
		cmd = write_ctrl_cabc;
		ret = dsi_send_mcs_short_hs(sender,
				cmd, 0x0, 1, DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* tear on*/
		cmd = set_tear_on;
		ret = dsi_send_mcs_short_hs(sender,
				cmd, 0x0, 1, DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* column address */
		cmd = set_column_address;
		ret = dsi_send_mcs_long_hs(sender,
				sharp10x19_mcs_column_addr, 5,
				DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* page address */
		cmd = set_page_addr;
		ret = dsi_send_mcs_long_hs(sender, sharp10x19_mcs_page_addr, 5,
				DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;
	}
	sender->work_for_slave_panel = false;
	return 0;

err_out:
	sender->work_for_slave_panel = false;
	pr_err("failed to send command %#x\n", cmd);
	return ret;
}

static void sharp10x19_cmd_controller_init(struct dsi_pipe *pipe)
{
	struct dsi_context *hw_ctx = &pipe->config.ctx;
	struct dsi_config *config = &pipe->config;

	pr_debug("\n");

	/*reconfig lane configuration*/
	pipe->config.lane_count = 4;
	pipe->config.lane_config = DSI_DATA_LANE_4_0;
	hw_ctx->cck_div = 1;
	hw_ctx->pll_bypass_mode = 0;

	hw_ctx->mipi_control = 0x0;
	hw_ctx->intr_en = 0xFFFFFFFF;
	hw_ctx->hs_tx_timeout = 0xFFFFFF;
	hw_ctx->lp_rx_timeout = 0xFFFFFF;
	hw_ctx->device_reset_timer = 0xffff;
	hw_ctx->turn_around_timeout = 0x14;

	if (is_dual_panel(config)) {
		hw_ctx->high_low_switch_count = 0x2B;
		hw_ctx->clk_lane_switch_time_cnt =  0x2b0014;
		hw_ctx->eot_disable = 0x0;
	} else {
		hw_ctx->high_low_switch_count = 0x2c;
		hw_ctx->clk_lane_switch_time_cnt =  0x2e0016;
		hw_ctx->eot_disable = EOT_DIS;
	}

	hw_ctx->lp_byteclk = 0x6;
	hw_ctx->dphy_param = 0x2a18681f;

	hw_ctx->init_count = 0xf0;
	hw_ctx->dbi_bw_ctrl = 1100;
	hw_ctx->hs_ls_dbi_enable = 0x0;
	hw_ctx->dsi_func_prg = DBI_16BIT_IF_OPTION_2 | pipe->config.lane_count;

	if (is_dual_panel(config))
		hw_ctx->mipi = SEL_FLOPPED_HSTX	| LP_OUTPUT_HOLD_ENABLE |
			DUAL_LINK_ENABLE | DUAL_LINK_CAPABLE;
	else
		hw_ctx->mipi = LP_OUTPUT_HOLD_ENABLE |
			BANDGAP_CHICKEN_BIT | TE_TRIGGER_BY_GPIO;

	hw_ctx->video_mode_format = DISABLE_VIDEO_BTA | IP_TG_CONFIG |
				    BURST_MODE;
}

static int sharp10x19_cmd_panel_connection_detect(struct dsi_pipe *pipe)
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

static int sharp10x19_cmd_power_on(struct dsi_pipe *pipe)
{
	int ret;
	u8 cmd;
	int i;
	int loop = 1;
	struct dsi_pkg_sender *sender = &pipe->sender;
	struct dsi_config *config = &pipe->config;

	pr_debug("\n");

	if (!sender) {
		pr_err("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	if (is_dual_panel(config))
		loop = 2;

	for (i = 0; i < loop; i++) {
		if (i == 0)
			sender->work_for_slave_panel = false;
		else
			sender->work_for_slave_panel = true;
		/* address mode */
		cmd = set_address_mode;
		ret = dsi_send_mcs_short_hs(sender,
				cmd, 0x0, 1, DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* pixel format*/
		cmd = set_pixel_format;
		ret = dsi_send_mcs_short_hs(sender,
				cmd, 0x77, 1, DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* display on */
		cmd = set_display_on;
		ret = dsi_send_mcs_short_hs(sender,
				cmd, 0, 0, DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;
	}
	sender->work_for_slave_panel = false;
	return 0;

err_out:
	sender->work_for_slave_panel = false;
	pr_err("failed to send command %#x\n", cmd);
	return ret;
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
		pr_err("%s: %d: failed to write vPro2\n",
				__func__, __LINE__);
}

static int sharp10x19_cmd_power_off(struct dsi_pipe *pipe)
{
	int err;
	int i;
	int loop = 1;
	struct dsi_pkg_sender *sender = &pipe->sender;
	struct dsi_config *config = &pipe->config;

	pr_debug("\n");

	if (!sender) {
		pr_err("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	if (is_dual_panel(config))
		loop = 2;

	for (i = 0; i < loop; i++) {
		if (i == 0)
			sender->work_for_slave_panel = false;
		else
			sender->work_for_slave_panel = true;

		err = dsi_send_mcs_short_hs(sender,
				set_display_off, 0, 0,
				DSI_SEND_PACKAGE);
		if (err) {
			pr_err("%s: %d: Set Display Off\n", __func__, __LINE__);
			goto power_off_err;
		}
		usleep_range(20000, 20100);

		err = dsi_send_mcs_short_hs(sender,
				set_tear_off, 0, 0,
				DSI_SEND_PACKAGE);
		if (err) {
			pr_err("%s: %d: Set Tear Off\n", __func__, __LINE__);
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

		err = dsi_send_gen_short_hs(sender, low_power_mode, 1, 2,
				DSI_SEND_PACKAGE);
		if (err) {
			pr_err("%s: %d: Set Low Power Mode\n",
							__func__, __LINE__);
			goto power_off_err;
		}
		if (bias_en_gpio)
			gpio_set_value_cansleep(bias_en_gpio, 0);
		usleep_range(1000, 1500);
	}
	sender->work_for_slave_panel = false;
	return 0;
power_off_err:
	sender->work_for_slave_panel = false;
	err = -EIO;
	return err;
}

static int sharp10x19_cmd_set_brightness(struct dsi_pipe *pipe, int level)
{
	u8 duty_val = 0;
	struct dsi_pkg_sender *sender = &pipe->sender;
	struct dsi_config *config = &pipe->config;

	pr_debug("%s: level = %d\n", __func__, level);

	if (!sender) {
		pr_err("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	duty_val = (0xFF * level) / 100;
	dsi_send_mcs_short_hs(sender,
			write_display_brightness, duty_val, 1,
			DSI_SEND_PACKAGE);

	if (is_dual_panel(config)) {
		sender->work_for_slave_panel = true;
		dsi_send_mcs_short_hs(sender,
				write_display_brightness, duty_val, 1,
				DSI_SEND_PACKAGE);
		sender->work_for_slave_panel = false;
	}
	return 0;
}

static void _get_panel_reset_gpio(bool is_dual_panel)
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
		pr_info("gpio_reseta=%d\n", mipi_reset_gpio);
	}
	if (is_dual_panel && (mipic_reset_gpio == 0)) {
		mipic_reset_gpio = ret;
		ret = gpio_request(mipic_reset_gpio, "mipic_display");
		if (ret) {
			pr_err("Faild to request panel reset gpio(c)\n");
			return;
		}
		gpio_direction_output(mipic_reset_gpio, 0);
		pr_info("gpio_resetc=%d\n", mipic_reset_gpio);
	}
}

static int sharp10x19_cmd_panel_reset(struct dsi_pipe *pipe)
{
	int ret = 0;
	u8 *vaddr = NULL, *vaddr1 = NULL;
	int reg_value_scl = 0;
	u8 i2_data[4];
	struct dsi_config *config = &pipe->config;

	pr_debug("\n");

	if (is_dual_panel(config)) {
		struct i2c_adapter *adapter =
			i2c_get_adapter(EXPANDER_BUS_NUMBER);
		if (adapter) {
			i2_data[0] = 0x4;
			i2_data[1] = 0x0;
			i2c_clients_command(adapter, 1, i2_data);
			i2_data[0] = 0x5;
			i2_data[1] = 0x3;
			i2c_clients_command(adapter, 1, i2_data);
		}
	}
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
		pr_info("gpio_bias_enable=%d\n", bias_en_gpio);
	}

	_get_panel_reset_gpio(is_dual_panel(config));

	gpio_direction_output(bias_en_gpio, 0);
	gpio_direction_output(mipi_reset_gpio, 0);
	if (is_dual_panel(config))
		gpio_direction_output(mipic_reset_gpio, 0);
	gpio_set_value_cansleep(bias_en_gpio, 0);
	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	if (is_dual_panel(config))
		gpio_set_value_cansleep(mipic_reset_gpio, 0);
	usleep_range(2000, 2500);
	gpio_set_value_cansleep(bias_en_gpio, 1);
	usleep_range(2000, 2500);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	usleep_range(2000, 2500);
	if (is_dual_panel(config)) {
		gpio_set_value_cansleep(mipic_reset_gpio, 1);
		usleep_range(3000, 3500);
	}
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
	return ret;
}

static int sharp10x19_cmd_exit_deep_standby(struct dsi_pipe *pipe)
{
	struct dsi_config *config = &pipe->config;

	pr_debug("\n");

	if (bias_en_gpio)
		gpio_set_value_cansleep(bias_en_gpio, 1);
	_get_panel_reset_gpio(is_dual_panel(config));
	gpio_direction_output(mipi_reset_gpio, 0);

	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	usleep_range(1000, 1500);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	usleep_range(3000, 3500);
	if (is_dual_panel(config)) {
		gpio_direction_output(mipic_reset_gpio, 0);
		gpio_set_value_cansleep(mipic_reset_gpio, 0);
		usleep_range(1000, 1500);
		gpio_set_value_cansleep(mipic_reset_gpio, 1);
		usleep_range(3000, 3500);
	}
	return 0;
}

static int sharp10x19_cmd_get_config_mode(struct drm_mode_modeinfo *mode)
{
	pr_debug("\n");

	if (!mode)
		return -EINVAL;

	mode->hdisplay = 1080;
	mode->hsync_start = mode->hdisplay + 8;
	mode->hsync_end = mode->hsync_start + 24;
	mode->htotal = mode->hsync_end + 8;

	mode->vdisplay = 1920;
	mode->vsync_start = 1923;
	mode->vsync_end = 1926;
	mode->vtotal = 1987;

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal * mode->htotal / 1000;

	adf_modeinfo_set_name(mode);

	return 0;
}

static int sharp10x19_dual_cmd_get_config_mode(struct drm_mode_modeinfo *mode)
{
	pr_debug("\n");

	if (!mode)
		return -EINVAL;

	mode->hdisplay = 2160;
	mode->hsync_start = mode->hdisplay + 8;
	mode->hsync_end = mode->hsync_start + 24;
	mode->htotal = mode->hsync_end + 8;

	mode->vdisplay = 1920;
	mode->vsync_start = 1923;
	mode->vsync_end = 1926;
	mode->vtotal = 1987;

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal * mode->htotal / 1000;

	adf_modeinfo_set_name(mode);

	return 0;
}

static void sharp10x19_cmd_get_panel_info(struct panel_info *pi)
{
	pr_debug("\n");

	pi->width_mm = SHARP_10x19_CMD_PHY_WIDTH;
	pi->height_mm = SHARP_10x19_CMD_PHY_HEIGHT;
	pi->bpp = SHARP_10x19_CMD_BPP;
	pi->dsi_type = SHARP_10x19_CMD_DSI_TYPE;
	pi->lane_num = SHARP_10x19_CMD_LANE_NUM;
	pi->dual_link = DSI_PANEL_SINGLE_LINK;
}

static void sharp10x19_dual_cmd_get_panel_info(struct panel_info *pi)
{
	pr_debug("\n");

	pi->width_mm = SHARP_10x19_CMD_PHY_WIDTH;
	pi->height_mm = SHARP_10x19_CMD_PHY_HEIGHT;
	pi->bpp = SHARP_10x19_CMD_BPP;
	pi->dsi_type = SHARP_10x19_CMD_DSI_TYPE;
	pi->lane_num = SHARP_10x19_CMD_LANE_NUM;
	pi->dual_link = DSI_PANEL_DUAL_PANEL;
}

static struct panel_ops sharp10x19_cmd_panel_ops = {
	.drv_ic_init = sharp10x19_cmd_drv_ic_init,
	.dsi_controller_init = sharp10x19_cmd_controller_init,
	.detect = sharp10x19_cmd_panel_connection_detect,
	.power_on = sharp10x19_cmd_power_on,
	.power_off = sharp10x19_cmd_power_off,
	.set_brightness = sharp10x19_cmd_set_brightness,
	.reset = sharp10x19_cmd_panel_reset,
	.get_config_mode = sharp10x19_cmd_get_config_mode,
	.exit_deep_standby = sharp10x19_cmd_exit_deep_standby,
	.get_panel_info = sharp10x19_cmd_get_panel_info,
};

struct dsi_panel sharp10x19_cmd_panel = {
	.panel_id = SHARP_10x19_CMD,
	.info.width_mm = SHARP_10x19_CMD_PHY_WIDTH,
	.info.height_mm = SHARP_10x19_CMD_PHY_HEIGHT,
	.info.bpp = SHARP_10x19_CMD_BPP,
	.info.dsi_type = SHARP_10x19_CMD_DSI_TYPE,
	.info.lane_num = SHARP_10x19_CMD_LANE_NUM,
	.info.dual_link = DSI_PANEL_SINGLE_LINK,
	.ops = &sharp10x19_cmd_panel_ops,
};

const struct dsi_panel *sharp_10x19_cmd_get_panel(void)
{
	return &sharp10x19_cmd_panel;
}

static struct panel_ops sharp10x19_dual_cmd_panel_ops = {
	.drv_ic_init = sharp10x19_cmd_drv_ic_init,
	.dsi_controller_init = sharp10x19_cmd_controller_init,
	.detect = sharp10x19_cmd_panel_connection_detect,
	.power_on = sharp10x19_cmd_power_on,
	.power_off = sharp10x19_cmd_power_off,
	.set_brightness = sharp10x19_cmd_set_brightness,
	.reset = sharp10x19_cmd_panel_reset,
	.get_config_mode = sharp10x19_dual_cmd_get_config_mode,
	.exit_deep_standby = sharp10x19_cmd_exit_deep_standby,
	.get_panel_info = sharp10x19_dual_cmd_get_panel_info,
};

struct dsi_panel sharp10x19_dual_cmd_panel = {
	.panel_id = SHARP_10x19_DUAL_CMD,
	.info.width_mm = SHARP_10x19_CMD_PHY_WIDTH,
	.info.height_mm = SHARP_10x19_CMD_PHY_HEIGHT,
	.info.bpp = SHARP_10x19_CMD_BPP,
	.info.dsi_type = SHARP_10x19_CMD_DSI_TYPE,
	.info.lane_num = SHARP_10x19_CMD_LANE_NUM,
	.info.dual_link = DSI_PANEL_DUAL_PANEL,
	.ops = &sharp10x19_dual_cmd_panel_ops,
};

const struct dsi_panel *sharp_10x19_dual_cmd_get_panel(void)
{
	return &sharp10x19_dual_cmd_panel;
}
