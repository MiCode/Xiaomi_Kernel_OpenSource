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
 *
 * Author: Jani Nikula <jani.nikula@intel.com>
 */

#include <drm/i915_drm.h>
#include <drm/i915_adf.h>
#include <intel_adf_device.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/dsi/dsi_config.h>
/*
 * FIXME:leaving for now till we can move all calls through
 * pipeline interface
 */
#include <core/vlv/vlv_dc_config.h>
/*
 * FIXME:leaving for now till we can move all calls through
 * pipeline interface
 */
#include <core/vlv/vlv_dc_gpio.h>
#include <core/common/intel_gen_backlight.h>
#include <linux/mfd/intel_soc_pmic.h>
#include "intel_dsi.h"
#include "intel_dsi_cmd.h"

#define PMIC_PANEL_EN		0x52
#define PMIC_PWM_EN		0x51
#define PMIC_BKL_EN		0x4B
#define PMIC_PWM_LEVEL		0x4E

void band_gap_reset(struct dsi_pipe *dsi_pipe)
{
	/* FIXME: move to pipeline */
	vlv_flisdsi_write(0x08, 0x0001);
	vlv_flisdsi_write(0x0F, 0x0005);
	vlv_flisdsi_write(0x0F, 0x0025);
	udelay(150);
	vlv_flisdsi_write(0x0F, 0x0000);
	vlv_flisdsi_write(0x08, 0x0000);
}

int intel_adf_dsi_soc_power_on(struct dsi_pipe *dsi_pipe)
{
	/*  cabc disable */
	vlv_gpio_write(IOSF_PORT_GPIO_NC,
			PANEL1_VDDEN_GPIONC_9_PCONF0, 0x2000CC00);
	vlv_gpio_write(IOSF_PORT_GPIO_NC,
			PANEL1_VDDEN_GPIONC_9_PAD, 0x00000004);

	/* panel enable */
	vlv_gpio_write(IOSF_PORT_GPIO_NC,
			PANEL1_BKLTCTL_GPIONC_11_PCONF0, 0x2000CC00);
	vlv_gpio_write(IOSF_PORT_GPIO_NC,
			PANEL1_BKLTCTL_GPIONC_11_PAD, 0x00000005);
	udelay(500);
	return 0;
}

int intel_adf_dsi_pmic_power_on(struct dsi_pipe *dsi_pipe)
{
	intel_soc_pmic_writeb(PMIC_PANEL_EN, 0x01);
	return 0;
}

int intel_adf_dsi_soc_power_off(struct dsi_pipe *dsi_pipe)
{
	vlv_gpio_write(IOSF_PORT_GPIO_NC,
			PANEL1_BKLTCTL_GPIONC_11_PCONF0, 0x2000CC00);
	vlv_gpio_write(IOSF_PORT_GPIO_NC,
			PANEL1_BKLTCTL_GPIONC_11_PAD, 0x00000004);
	udelay(500);
	return 0;
}

int intel_adf_dsi_pmic_power_off(struct dsi_pipe *dsi_pipe)
{
	intel_soc_pmic_writeb(PMIC_PANEL_EN, 0x00);
	return 0;
}

int intel_adf_dsi_pmic_backlight_on(struct dsi_pipe *dsi_pipe)
{
	intel_soc_pmic_writeb(PMIC_BKL_EN, 0xFF);
	intel_soc_pmic_writeb(PMIC_PWM_EN, 0x01);

	panel_generic_enable_bklt(dsi_pipe);
	return 0;
}

int intel_adf_dsi_soc_backlight_on(struct dsi_pipe *dsi_pipe)
{
	vlv_gpio_write(IOSF_PORT_GPIO_NC,
			PANEL1_BKLTEN_GPIONC_10_PCONF0, 0x2000CC00);
	vlv_gpio_write(IOSF_PORT_GPIO_NC,
			PANEL1_BKLTEN_GPIONC_10_PAD, 0x00000005);
	udelay(500);

	return 0;
}

int intel_adf_dsi_pmic_backlight_off(struct dsi_pipe *dsi_pipe)
{
	panel_generic_disable_bklt(dsi_pipe);

	intel_soc_pmic_writeb(PMIC_PWM_EN, 0x00);
	intel_soc_pmic_writeb(PMIC_BKL_EN, 0x7F);

	return 0;
}

int intel_adf_dsi_soc_backlight_off(struct dsi_pipe *dsi_pipe)
{
	vlv_gpio_write(IOSF_PORT_GPIO_NC,
			PANEL1_BKLTEN_GPIONC_10_PCONF0, 0x2000CC00);
	vlv_gpio_write(IOSF_PORT_GPIO_NC,
			PANEL1_BKLTEN_GPIONC_10_PAD, 0x00000004);
	udelay(500);
	return 0;
}

void adf_dsi_hs_mode_enable(struct dsi_pipe *dsi_pipe, bool enable)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_hs_mode_enable(pipeline, enable);
}

/* XXX: questionable write helpers */
int adf_dsi_vc_dcs_write(struct dsi_pipe *dsi_pipe, int channel,
		const u8 *data, int len, enum port port)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_dcs_write(pipeline, channel, data, len, port);
}

int adf_dsi_vc_generic_write(struct dsi_pipe *dsi_pipe, int channel,
		const u8 *data, int len, enum port port)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_generic_write(pipeline, channel, data, len, port);
}

/* XXX: questionable read helpers */
int adf_dsi_vc_dcs_read(struct dsi_pipe *dsi_pipe, int channel, u8 dcs_cmd,
		u8 *buf, int buflen, enum port port)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_dcs_read(pipeline, channel, dcs_cmd,
		buf, buflen, port);
}

int adf_dsi_vc_generic_read(struct dsi_pipe *dsi_pipe, int channel,
		u8 *reqdata, int reqlen, u8 *buf, int buflen, enum port port)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_generic_read(pipeline, channel, reqdata,
		reqlen, buf, buflen, port);
}

static inline int adf_dsi_vc_generic_read_0(struct dsi_pipe *dsi_pipe,
		int channel, u8 *buf, int buflen, enum port port)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_generic_read(pipeline, channel, NULL, 0,
		buf, buflen, port);
}

static inline int adf_dsi_vc_generic_read_1(struct dsi_pipe *dsi_pipe,
					int channel, u8 param, u8 *buf,
					int buflen, enum port port)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_generic_read(pipeline, channel, &param, 1,
		buf, buflen, port);
}

static inline int adf_dsi_vc_generic_read_2(struct dsi_pipe *dsi_pipe,
					int channel, u8 param1, u8 param2,
					u8 *buf, int buflen, enum port port)
{
	u8 req[2] = { param1, param2 };
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_generic_read(pipeline, channel, req, 2,
		buf, buflen, port);
}

int adf_dpi_send_cmd(struct dsi_pipe *dsi_pipe, u32 cmd, bool hs)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_dpi_send_cmd(pipeline, cmd, hs);
}

inline int adf_dsi_vc_dcs_write_0(struct dsi_pipe *dsi_pipe,
				int channel, u8 dcs_cmd, enum port port)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_dcs_write(pipeline, channel, &dcs_cmd, 1, port);
}

inline int adf_dsi_vc_dcs_write_1(struct dsi_pipe *dsi_pipe,
			int channel, u8 dcs_cmd, u8 param, enum port port)
{
	u8 buf[2] = { dcs_cmd, param };
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_dcs_write(pipeline, channel, buf, 2, port);
}

inline int adf_dsi_vc_generic_write_0(struct dsi_pipe *dsi_pipe,
				int channel, enum port port)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_generic_write(pipeline, channel, NULL, 0, port);
}

inline int adf_dsi_vc_generic_write_1(struct dsi_pipe *dsi_pipe,
					int channel, u8 param, enum port port)
{
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_generic_write(pipeline, channel, &param, 1, port);
}

inline int adf_dsi_vc_generic_write_2(struct dsi_pipe *dsi_pipe,
			int channel, u8 param1, u8 param2, enum port port)
{
	u8 buf[2] = { param1, param2 };
	struct intel_pipeline *pipeline = dsi_pipe->base.pipeline;
	return vlv_cmd_vc_generic_write(pipeline, channel, buf, 2, port);
}
