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

#define INTEL_SIDEBAND_REG_READ		0
#define INTEL_SIDEBAND_REG_WRITE	1

extern void vlv_enable_dsi_pll(struct dsi_config *);
extern void vlv_disable_dsi_pll(struct dsi_config *);
extern u32 vlv_get_dsi_pclk(struct dsi_config *, int pipe_bpp);

extern int intel_dsi_pre_enable(struct dsi_pipe *dsi_pipe);
extern void intel_dsi_pre_post(struct dsi_pipe *dsi_pipe);
extern int intel_dsi_post_disable(struct dsi_pipe *dsi_pipe);
extern int intel_dsi_prepare(struct dsi_pipe *dsi_pipe,
						struct drm_mode_modeinfo *mode);
extern int intel_dsi_set_events(struct dsi_pipe *dsi_pipe, u8 event,
						bool enabled);
extern void intel_dsi_get_events(struct dsi_pipe *dsi_pipe, u32 *events);
extern void intel_dsi_handle_events(struct dsi_pipe *dsi_pipe, u32 events);
extern void intel_dsi_pre_disable(struct dsi_pipe *dsi_pipe);
extern int intel_dsi_modeset(struct dsi_pipe *dsi_pipe,
						struct drm_mode_modeinfo *mode);

extern int intel_dsi_soc_power_on(struct dsi_pipe *dsi_pipe);
extern int intel_dsi_pmic_power_on(struct dsi_pipe *dsi_pipe);
extern int intel_dsi_soc_power_off(struct dsi_pipe *dsi_pipe);
extern int intel_dsi_pmic_power_off(struct dsi_pipe *dsi_pipe);
extern int intel_dsi_pmic_backlight_on(struct dsi_pipe *dsi_pipe);
extern int intel_dsi_soc_backlight_on(struct dsi_pipe *dsi_pipe);
extern int intel_dsi_pmic_backlight_off(struct dsi_pipe *dsi_pipe);
extern int intel_dsi_soc_backlight_off(struct dsi_pipe *dsi_pipe);
extern int generic_enable_bklt(struct dsi_pipe *interface);
extern int generic_disable_bklt(struct dsi_pipe *interface);
#endif /* _INTEL_DSI_H */
