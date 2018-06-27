/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MDSS_SPI_PANEL_H__
#define __MDSS_SPI_PANEL_H__

#if defined(CONFIG_FB_MSM_MDSS_SPI_PANEL) && defined(CONFIG_SPI_QUP)
#include <linux/list.h>
#include <linux/mdss_io_util.h>
#include <linux/irqreturn.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>

#include "mdss_panel.h"
#include "mdp3_dma.h"

#define MDSS_MAX_BL_BRIGHTNESS 255

#define MDSS_SPI_RST_SEQ_LEN	10

#define NONE_PANEL "none"

#define CTRL_STATE_UNKNOWN		0x00
#define CTRL_STATE_PANEL_INIT		BIT(0)
#define CTRL_STATE_MDP_ACTIVE		BIT(1)

#define MDSS_PINCTRL_STATE_DEFAULT "mdss_default"
#define MDSS_PINCTRL_STATE_SLEEP  "mdss_sleep"
#define SPI_PANEL_TE_TIMEOUT	400

enum spi_panel_data_type {
	panel_cmd,
	panel_parameter,
	panel_pixel,
	UNKNOWN_FORMAT,
};

enum spi_panel_bl_ctrl {
	SPI_BL_PWM,
	SPI_BL_WLED,
	SPI_BL_DCS_CMD,
	SPI_UNKNOWN_CTRL,
};

struct spi_pinctrl_res {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};
#define SPI_PANEL_DST_FORMAT_RGB565		0

struct spi_ctrl_hdr {
	char wait;	/* ms */
	char dlen;	/* 8 bits */
};

struct spi_cmd_desc {
	struct spi_ctrl_hdr dchdr;
	char *command;
	char *parameter;
};

struct spi_panel_cmds {
	char *buf;
	int blen;
	struct spi_cmd_desc *cmds;
	int cmd_cnt;
};

enum spi_panel_status_mode {
	SPI_ESD_REG,
	SPI_SEND_PANEL_COMMAND,
	SPI_ESD_MAX,
};


struct spi_panel_data {
	struct mdss_panel_data panel_data;
	struct mdss_util_intf *mdss_util;
	struct spi_pinctrl_res pin_res;
	struct mdss_module_power panel_power_data;
	struct completion spi_panel_te;
	struct mdp3_notification vsync_client;
	unsigned int vsync_status;
	int byte_pre_frame;
	char *tx_buf;
	u8 ctrl_state;
	int disp_te_gpio;
	int rst_gpio;
	int disp_dc_gpio;	/* command or data */
	struct spi_panel_cmds on_cmds;
	struct spi_panel_cmds off_cmds;
	bool (*check_status)(struct spi_panel_data *pdata);
	int (*on)(struct mdss_panel_data *pdata);
	int (*off)(struct mdss_panel_data *pdata);
	struct mutex spi_tx_mutex;
	struct pwm_device *pwm_bl;
	int bklt_ctrl;	/* backlight ctrl */
	bool pwm_pmi;
	int pwm_period;
	int pwm_pmic_gpio;
	int pwm_lpg_chan;
	int pwm_enabled;
	int bklt_max;
	int status_mode;
	u32 status_cmds_rlen;
	u8 panel_status_reg;
	u8 *exp_status_value;
	u8 *act_status_value;
	unsigned char *return_buf;
};

int mdss_spi_panel_kickoff(struct mdss_panel_data *pdata,
				char *buf, int len, int stride);
int is_spi_panel_continuous_splash_on(struct mdss_panel_data *pdata);
void mdp3_spi_vsync_enable(struct mdss_panel_data *pdata,
				struct mdp3_notification *vsync_client);
void mdp3_check_spi_panel_status(struct work_struct *work,
				uint32_t interval);

#else
static inline int mdss_spi_panel_kickoff(struct mdss_panel_data *pdata,
				char *buf, int len, int stride){
	return 0;
}
static inline int is_spi_panel_continuous_splash_on(
				struct mdss_panel_data *pdata)
{
	return 0;
}
static inline int mdp3_spi_vsync_enable(struct mdss_panel_data *pdata,
			struct mdp3_notification *vsync_client){
	return 0;
}

#endif/* End of CONFIG_FB_MSM_MDSS_SPI_PANEL && ONFIG_SPI_QUP */

#endif /* End of __MDSS_SPI_PANEL_H__ */
