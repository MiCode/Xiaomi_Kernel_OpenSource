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
 */

#include <drm/i915_drm.h>
#include <intel_adf_device.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/dsi/dsi_config.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_dc_gpio.h>
#include <core/common/dsi/dsi_pipe.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/pwm.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/delay.h>
#include <core/common/backlight_dev.h>
#include "dsi/dsi_vbt.h"

#define PMIC_PWM_LEVEL 0x4E

static void lpio_set_backlight(u32 level)
{
	pr_debug("%s setting backlight level = %d\n", __func__, level);

	/* FixMe: if level is zero still a pulse is observed consuming
	power. To fix this issue if requested level is zero then
	disable pwm and enabled it again if brightness changes */
	lpio_bl_write_bits(0, LPIO_PWM_CTRL, (0xff - level), 0xFF);
	lpio_bl_update(0, LPIO_PWM_CTRL);
}

static inline u32 lpio_get_backlight(void)
{
	return lpio_bl_read(0, LPIO_PWM_CTRL) & 0xff;
}

static inline u32 pmic_get_backlight(void)
{
	return intel_soc_pmic_readb(PMIC_PWM_LEVEL);
}

static inline void pmic_set_backlight(u32 level)
{
	pr_debug("%s setting backlight level = %d\n", __func__, level);
	intel_soc_pmic_writeb(PMIC_PWM_LEVEL, level);
}

void intel_backlight_init(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi = to_dsi_pipe(pipe);
	struct dsi_vbt *vbt = dsi->config.dsi;

	pr_debug("%s: Backlight initialized for pipe = %s\n", __func__,
			pipe->type == INTEL_PIPE_DSI ? "DSI" : "UNSUPPORTED");

	if (vbt->config->pwm_blc) {
		dsi->ops.set_brightness = lpio_set_backlight;
		dsi->ops.get_brightness = lpio_get_backlight;
	} else {
		dsi->ops.set_brightness = pmic_set_backlight;
		dsi->ops.get_brightness = pmic_get_backlight;
	}

	dsi->config.ctx.backlight_level = BRIGHTNESS_INIT_LEVEL *
						0xFF / BRIGHTNESS_MAX_LEVEL;
}

static uint32_t compute_pwm_base(uint16_t freq)
{
	uint32_t base_unit;

	if (freq < 400)
		freq = 400;
	/*The PWM block is clocked by the 25MHz oscillator clock.
	* The output frequency can be estimated with the equation:
	* Target frequency = XOSC * Base_unit_value/256
	*/
	base_unit = (freq * 256) / 25;

	/* Also Base_unit_value need to converted to QM.N notation
	* to program the value in register
	* Using the following for converting to Q8.8 notation
	* For QM.N representation, consider a floating point variable 'a' :
	* Step 1: Calculate b = a* 2^N , where N is the fractional length
	* of the variable.
	* Note that a is represented in decimal.
	* Step 2: Round the value of 'b' to the nearest integer value.
	* For example:
	* RoundOff (1.05) --> 1
	* RoundOff (1.5)  --> 2
	* Step 3: Convert 'b' from decimal to binary representation and name
	* the new variable 'c'
	*/
	base_unit = base_unit * 256;
	base_unit = DIV_ROUND_CLOSEST(base_unit, 1000000);

	return base_unit;
}

void lpio_enable_backlight(void)
{
	uint32_t pwm_base, pwm_freq;

	vlv_gps_core_read(0x40A0);
	vlv_gps_core_write(0x40A0, 0x2000CC01);
	vlv_gps_core_write(0x40A8, 0x5);

	/* PWM enable
	* Assuming only 1 LFP
	*/
	pwm_freq = intel_adf_get_pwm_vbt_data();
	pwm_base = compute_pwm_base(pwm_freq);
	pwm_base = pwm_base << 8;
	lpio_bl_write(0, LPIO_PWM_CTRL, pwm_base);
	lpio_bl_update(0, LPIO_PWM_CTRL);
	lpio_bl_write_bits(0, LPIO_PWM_CTRL, 0x80000000, 0x80000000);
	lpio_bl_update(0, LPIO_PWM_CTRL);
}

void intel_enable_backlight(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_vbt *vbt;
	u32 level = dsi_pipe->config.ctx.backlight_level;

	vbt = dsi_pipe->config.dsi;

	pr_debug("%s\n", __func__);

	if (vbt->config->pwm_blc)
		lpio_enable_backlight();

	if (dsi_pipe->panel->ops->enable_backlight)
		dsi_pipe->panel->ops->enable_backlight(dsi_pipe);

	/* set the last backlight level */
	dsi_pipe->ops.set_brightness(level);
}

static inline void lpio_disable_backlight(void)
{
	lpio_bl_write_bits(0, LPIO_PWM_CTRL, 0x00, 0x80000000);
}

void intel_disable_backlight(struct intel_pipe *pipe)
{
	struct dsi_pipe *dsi_pipe = to_dsi_pipe(pipe);
	struct dsi_vbt *vbt;

	pr_debug("%s\n", __func__);

	vbt = dsi_pipe->config.dsi;

	/* set the backlight to 0 first */
	dsi_pipe->ops.set_brightness(0);

	if (dsi_pipe->panel->ops->disable_backlight)
		dsi_pipe->panel->ops->disable_backlight(dsi_pipe);

	if (vbt->config->pwm_blc)
		lpio_disable_backlight();
}
