/**************************************************************************
 *
 * Copyright 2013 Intel Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Author:
 * Deepak S <deepak.s@intel.com>
 *
 **************************************************************************/

#ifndef _I915_ADF_WRAPPER_H_
#define _I915_ADF_WRAPPER_H_

#include <drm/drm_crtc.h>

#define INTEL_SIDEBAND_REG_READ		0
#define INTEL_SIDEBAND_REG_WRITE	1

extern void intel_adf_dpio_sideband_rw(u32 operation, u32 port,
				       u32 reg, u32 *val);
extern void intel_adf_pci_sideband_rw(u32 operation, u32 port,
				      u32 reg, u32 *val);
extern struct pci_dev *i915_adf_get_pci_dev(void);
extern void intel_adf_get_dsi_vbt_data(void **vbt_data,
				   struct drm_display_mode **mode);
extern u32 intel_adf_get_pwm_vbt_data(void);
extern void set_adf_ready(void);
extern volatile bool g_adf_ready;
extern int intel_adf_context_on_event(void);
extern void intel_adf_display_rpm_get(void);
extern void intel_adf_display_rpm_put(void);
extern int i915_adf_driver_initialize(void);
extern u8 intel_adf_get_platform_id(void);
extern u16 intel_get_dsi_port_frm_vbt(void);
extern void intel_get_vbt_disp_conf(void **child_dev, int *child_dev_num);
extern struct i2c_adapter *intel_adf_get_gmbus_adapter(u8 port);
extern unsigned short *intel_get_vbt_pps_delays(void);
extern u32 intel_get_vbt_drrs_support(void);
extern u32 intel_get_vbt_drrs_min_vrefresh(void);

/* ADF register calls for audio driver */
int adf_hdmi_audio_register(void *drv, void *had_data);
int adf_hdmi_audio_setup(void *callbacks, void *r_ops, void *q_ops);
#endif


