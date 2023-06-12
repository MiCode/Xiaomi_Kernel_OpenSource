/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2014-2015, 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __QPIC_DISPLAY_H__
#define __QPIC_DISPLAY_H__

#include <linux/clk.h>
#include <linux/msm-sps.h>
#include <linux/interrupt.h>
#include <linux/interconnect.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <drm/drm_drv.h>
#include <drm/drm_connector.h>
#include <drm/drm_simple_kms_helper.h>

#define QPIC_REG_QPIC_LCDC_CTRL				0x22000
#define QPIC_REG_LCDC_VERSION				0x22004
#define QPIC_REG_QPIC_LCDC_IRQ_EN			0x22008
#define QPIC_REG_QPIC_LCDC_IRQ_STTS			0x2200C
#define QPIC_REG_QPIC_LCDC_IRQ_CLR			0x22010
#define QPIC_REG_QPIC_LCDC_STTS				0x22014
#define QPIC_REG_QPIC_LCDC_CMD_DATA_CYCLE_CNT		0x22018
#define QPIC_REG_QPIC_LCDC_CFG0				0x22020
#define QPIC_REG_QPIC_LCDC_CFG1				0x22024
#define QPIC_REG_QPIC_LCDC_CFG2				0x22028
#define QPIC_REG_QPIC_LCDC_RESET			0x2202C
#define QPIC_REG_QPIC_LCDC_FIFO_SOF			0x22100
#define QPIC_REG_LCD_DEVICE_CMD0			0x23000
#define QPIC_REG_QPIC_LCDC_FIFO_DATA_PORT0		0x22140
#define QPIC_REG_QPIC_LCDC_FIFO_EOF			0x22180

#define QPIC_OUTP(qpic_display, off, data) \
	writel_relaxed((data), (qpic_display)->qpic_base  + (off))
#define QPIC_OUTPW(qpic_display, off, data) \
	writew_relaxed((data), (qpic_display)->qpic_base  + (off))
#define QPIC_INP(qpic_display, off) \
	readl_relaxed((qpic_display)->qpic_base + (off))

#define QPIC_MAX_VSYNC_WAIT_TIME_IN_MS			500

#define QPIC_MAX_CMD_BUF_SIZE_IN_BYTES			512
#define QPIC_PINCTRL_STATE_DEFAULT "qpic_display_default"
#define QPIC_PINCTRL_STATE_SLEEP  "qpic_display_sleep"

/* Structure that defines an SPS end point for a BAM pipe. */
struct qpic_sps_endpt {
	struct sps_pipe *handle;
	struct sps_connect config;
	struct sps_register_event bam_event;
	struct completion completion;
};

struct qpic_panel_config {
	u32 xres;
	u32 yres;
	u32 bpp;
	u32 type;
};

struct qpic_pinctrl_res {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};

struct qpic_panel_io_desc {
	int rst_gpio;
	int cs_gpio;
	int ad8_gpio;
	int te_gpio;
	int bl_gpio;
	struct regulator *vdd_vreg;
	struct regulator *avdd_vreg;
	struct qpic_pinctrl_res pin_res;
};

struct bus_scaling_data {
	u64 ab;
	u64 ib;
};

struct msm_bus_path {
	unsigned int num_paths;
	struct bus_scaling_data *vec;
};

struct qpic_display_bus_scale_pdata {
	const char *name;
	unsigned int num_usecase;
	struct msm_bus_path *usecase;
	struct icc_path *data_bus_hdl;
	u32 curr_vote;
};

struct qpic_display_data {
	u32 rev;
	struct platform_device *pdev;
	struct drm_device drm_dev;
	struct drm_simple_display_pipe pipe;
	struct drm_connector conn;
	struct drm_display_mode drm_mode;
	bool pipe_enabled;

	size_t qpic_reg_size;
	u32 qpic_phys;
	char __iomem *qpic_base;
	u32 irq_id;
	bool irq_ena;
	u32 res_init;

	void *cmd_buf_virt;
	phys_addr_t cmd_buf_phys;
	struct qpic_sps_endpt qpic_endpt;
	u32 sps_init;
	u32 irq_requested;
	struct qpic_display_bus_scale_pdata *data_bus_pdata;
	struct completion fifo_eof_comp;
	bool is_qpic_on;
	struct clk *qpic_clk;
	struct clk *qpic_a_clk;
	int (*qpic_transfer)(struct qpic_display_data *qpic_display,
			u32 cmd, u8 *param, u32 len);

	bool is_panel_on;
	struct qpic_panel_config *panel_config;
	struct qpic_panel_io_desc panel_io;
	int (*panel_on)(struct qpic_display_data *qpic_display);
	void (*panel_off)(struct qpic_display_data *qpic_display);

};

void get_ili_qvga_panel_config(struct qpic_display_data *qpic_display);

#endif
