/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __GEN_HDMI_HOTPLUG__
#define __GEN_HDMI_HOTPLUG__

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <drm/drm_crtc.h>
#include <drm/drm_modes.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/chv_dc_regs.h>
#include <intel_adf.h>

/* Extern variables */
extern struct drm_display_mode *fallback_modes;
extern const struct intel_adf_context *g_adf_context;

/* EDID parsing and monitor detection in gen_hdmi_edid.c*/
extern bool hdmi_notify_audio(struct hdmi_pipe *hdmi_pipe, bool connected);
extern bool rgb_quant_range_selectable(struct edid *edid);
extern  u32 edid_get_quirks(struct edid *edid);
extern void edid_to_eld(struct hdmi_monitor *monitor, struct edid *edid);

extern int
add_cvt_modes(struct hdmi_monitor *monitor, struct edid *edid);
extern int
add_detailed_modes(struct hdmi_monitor *monitor, struct edid *edid, u32 quirks);
extern int
add_standard_modes(struct hdmi_monitor *monitor, struct edid *edid);
extern int
add_established_modes(struct hdmi_monitor *monitor, struct edid *edid);
extern int
add_inferred_modes(struct hdmi_monitor *monitor, struct edid *edid);
extern int
add_cea_modes(struct hdmi_monitor *monitor, struct edid *edid);
extern int
add_alternate_cea_modes(struct hdmi_monitor *monitor, struct edid *edid);
extern void
edid_fixup_preferred(struct hdmi_monitor *monitor, u32 quirks);

#endif
