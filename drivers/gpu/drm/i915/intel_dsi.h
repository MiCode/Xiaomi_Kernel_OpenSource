/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _INTEL_DSI_H
#define _INTEL_DSI_H

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include "intel_drv.h"

#define PMIC_PANEL_EN		0x52
#define PMIC_PWM_EN		0x51
#define PMIC_BKL_EN		0x4B
#define PMIC_PWM_LEVEL		0x4E

#define GPI0_NC_0_HV_DDI0_HPD		0x4130
#define GPIO_NC_0_HV_DDI0_PAD		0x4138
#define GPIO_NC_1_HV_DDI0_DDC_SDA	0x4120
#define GPIO_NC_1_HV_DDI0_DDC_SDA_PAD	0x4128
#define GPIO_NC_2_HV_DDI0_DDC_SCL	0x4110
#define GPIO_NC_2_HV_DDI0_DDC_SCL_PAD	0x4118
#define GPIO_NC_3_PANEL0_VDDEN		0x4140
#define GPIO_NC_3_PANEL0_VDDEN_PAD	0x4148
#define GPIO_NC_4_PANEL0_BLKEN		0x4150
#define GPIO_NC_4_PANEL0_BLKEN_PAD	0x4158
#define GPIO_NC_5_PANEL0_BLKCTL		0x4160
#define GPIO_NC_5_PANEL0_BLKCTL_PAD	0x4168
#define GPIO_NC_6_PCONF0		0x4180
#define GPIO_NC_6_PAD			0x4188
#define GPIO_NC_7_PCONF0		0x4190
#define GPIO_NC_7_PAD			0x4198
#define GPIO_NC_8_PCONF0		0x4170
#define GPIO_NC_8_PAD			0x4178
#define GPIO_NC_9_PCONF0		0x4100
#define GPIO_NC_9_PAD			0x4108
#define GPIO_NC_10_PCONF0		0x40E0
#define GPIO_NC_10_PAD			0x40E8
#define GPIO_NC_11_PCONF0		0x40F0
#define GPIO_NC_11_PAD			0x40F8
#define GPIO_NC_22_PCONF0		0x40A0
#define GPIO_NC_22_PAD			0x40A8

struct intel_dsi_device {
	unsigned int panel_id;
	const char *name;
	const struct intel_dsi_dev_ops *dev_ops;
	void *dev_priv;
};

struct intel_dsi_dev_ops {
	bool (*init)(struct intel_dsi_device *dsi);

	void (*panel_reset)(struct intel_dsi_device *dsi);

	void (*disable_panel_power)(struct intel_dsi_device *dsi);

	/* one time programmable commands if needed */
	void (*send_otp_cmds)(struct intel_dsi_device *dsi);

	/* This callback must be able to assume DSI commands can be sent */
	void (*enable)(struct intel_dsi_device *dsi);

	/* This callback must be able to assume DSI commands can be sent */
	void (*disable)(struct intel_dsi_device *dsi);

	int (*mode_valid)(struct intel_dsi_device *dsi,
			  struct drm_display_mode *mode);

	bool (*mode_fixup)(struct intel_dsi_device *dsi,
			   const struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);

	void (*mode_set)(struct intel_dsi_device *dsi,
			 struct drm_display_mode *mode,
			 struct drm_display_mode *adjusted_mode);

	enum drm_connector_status (*detect)(struct intel_dsi_device *dsi);

	bool (*get_hw_state)(struct intel_dsi_device *dev);

	struct drm_display_mode *(*get_modes)(struct intel_dsi_device *dsi);

	void (*destroy) (struct intel_dsi_device *dsi);
};

struct intel_dsi {
	struct intel_encoder base;

	struct intel_dsi_device dev;

	struct intel_connector *attached_connector;

	/* if true, use HS mode, otherwise LP */
	bool hs;

	/* virtual channel */
	int channel;

	/* Video mode or command mode */
	u16 operation_mode;

	/* number of DSI lanes */
	unsigned int lane_count;

	/* video mode pixel format for MIPI_DSI_FUNC_PRG register */
	u32 pixel_format;

	/* video mode format for MIPI_VIDEO_MODE_FORMAT register */
	u32 video_mode_format;

	/* eot for MIPI_EOT_DISABLE register */
	u8 eotp_pkt;
	u8 clock_stop;

	u8 escape_clk_div;
	u32 port_bits;
	u32 bw_timer;
	u32 dphy_reg;
	u32 video_frmt_cfg_bits;
	u16 lp_byte_clk;

	/* timeouts in byte clocks */
	u16 lp_rx_timeout;
	u16 turn_arnd_val;
	u16 rst_timer_val;
	u16 hs_to_lp_count;
	u16 clk_lp_to_hs_count;
	u16 clk_hs_to_lp_count;

	u16 init_count;

	/* all delays in ms */
	u16 backlight_off_delay;
	u16 backlight_on_delay;
	u16 panel_on_delay;
	u16 panel_off_delay;
	u16 panel_pwr_cycle_delay;
};

static inline struct intel_dsi *enc_to_intel_dsi(struct drm_encoder *encoder)
{
	return container_of(encoder, struct intel_dsi, base.base);
}

extern void vlv_enable_dsi_pll(struct intel_encoder *encoder);
extern void vlv_disable_dsi_pll(struct intel_encoder *encoder);

extern struct intel_dsi_dev_ops vbt_generic_dsi_display_ops;

#endif /* _INTEL_DSI_H */
