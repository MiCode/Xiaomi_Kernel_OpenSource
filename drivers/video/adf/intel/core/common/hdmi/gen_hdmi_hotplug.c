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
 *
 * Create on 12 Dec 2014
 * Author: Shashank Sharma <shashank.sharma@intel.com>
 */

#include <core/common/hdmi/gen_hdmi_pipe.h>
#include <core/common/hdmi/gen_hdmi_hotplug.h>
#include "hdmi_edid.h"

struct drm_display_mode fallback_mode = {
	DEFINE_MODE("1920x1080", DRM_MODE_TYPE_DRIVER,
	148500, 1920, 2008, 2052, 2200, 0, 1080, 1084,
	1089, 1125, 0, DRM_MODE_FLAG_PHSYNC | DRM_MODE_FLAG_PVSYNC),
	.vrefresh = 60,
};

#ifdef HDMI_USE_FALLBACK_EDID
static u8 fallback_edid[EDID_LENGTH] = {
	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
	0x10, 0xAC, 0x8A, 0xA0, 0x51, 0x45, 0x4E, 0x30,
	0x2C, 0x16, 0x01, 0x03, 0x0E, 0x35, 0x1E, 0x78,
	0xEA, 0x2F, 0x15, 0xA5, 0x55, 0x55, 0x9F, 0x28,
	0x0D, 0x50, 0x54, 0xA5, 0x4B, 0x00, 0x71, 0x4F,
	0x81, 0x80, 0xD1, 0xC0, 0x01, 0x01, 0x01, 0x01,
	0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x3A,
	0x80, 0x18, 0x71, 0x38, 0x2D, 0x40, 0x58, 0x2C,
	0x45, 0x00, 0x13, 0x2B, 0x21, 0x00, 0x00, 0x1E,
	0x00, 0x00, 0x00, 0xFF, 0x00, 0x30, 0x4A, 0x56,
	0x44, 0x52, 0x32, 0x42, 0x33, 0x30, 0x4E, 0x45,
	0x51, 0x0A, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x44,
	0x45, 0x4C, 0x4C, 0x20, 0x53, 0x32, 0x34, 0x34,
	0x30, 0x4C, 0x0A, 0x20, 0x00, 0x00, 0x00, 0xFD,
	0x00, 0x38, 0x4C, 0x1E, 0x53, 0x11, 0x00, 0x0A,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0xB5
};
#endif

int _parse_edid(struct hdmi_monitor *monitor, struct edid *raw_edid)
{
	int num_modes = 0;
	u32 quirks;

	if (raw_edid == NULL) {
		pr_err("ADF: HDMI: %s: EDID NULL.\n", __func__);
		return -EINVAL;
	}

	if (!edid_is_valid(raw_edid)) {
		pr_err("ADF: HDMI: %s: EDID invalid.\n", __func__);
		return -EINVAL;
	}

	quirks = edid_get_quirks(raw_edid);

	/*
	 * detailed modes are those described in 18-byte chunks,
	 * both standard and extended
	 */

	num_modes += add_detailed_modes(monitor, raw_edid, quirks);
	num_modes += add_cvt_modes(monitor, raw_edid);
	num_modes += add_standard_modes(monitor, raw_edid);
	num_modes += add_established_modes(monitor, raw_edid);

	if (raw_edid->features & DRM_EDID_FEATURE_DEFAULT_GTF)
		num_modes += add_inferred_modes(monitor, raw_edid);
	num_modes += add_cea_modes(monitor, raw_edid);
	num_modes += add_alternate_cea_modes(monitor, raw_edid);


	if (quirks & (EDID_QUIRK_PREFER_LARGE_60 | EDID_QUIRK_PREFER_LARGE_75))
		edid_fixup_preferred(monitor, quirks);

	edid_to_eld(monitor, raw_edid);
	monitor->quant_range_selectable = rgb_quant_range_selectable(raw_edid);
	monitor->screen_width_mm = raw_edid->width_cm * 10;
	monitor->screen_height_mm = raw_edid->height_cm * 10;
	pr_info("In %s and width_cm=%d, height_cm=%d\n", __func__,
		raw_edid->width_cm, raw_edid->height_cm);

	pr_info("ADF: HDMI: %s found %d modes in EDID\n", __func__, num_modes);
	return num_modes;
}

/* Utility function */
static struct vlv_hdmi_port*
_hdmi_port_from_pipe(struct hdmi_pipe *hdmi_pipe)
{
	struct intel_pipeline *intel_pipeline = hdmi_pipe->base.pipeline;
	struct vlv_pipeline *pipeline = to_vlv_pipeline(intel_pipeline);
	return &pipeline->port.hdmi_port;
}

/* Must free the returned pointer after use */
struct drm_mode_modeinfo *
_display_mode_to_modeinfo(struct drm_display_mode *mode)
{
	struct drm_mode_modeinfo *modeinfo;
	if (!mode) {
		pr_err("%s: NULL input mode\n", __func__);
		return NULL;
	}

	modeinfo = (struct drm_mode_modeinfo *)
		kzalloc(sizeof(struct drm_mode_modeinfo), GFP_KERNEL);
	if (!modeinfo) {
		pr_err("%s: OOM\n", __func__);
		return NULL;
	}

	modeinfo->clock = mode->clock;
	modeinfo->hdisplay = (u16) mode->hdisplay;
	modeinfo->hsync_start = (u16) mode->hsync_start;
	modeinfo->hsync_end = (u16) mode->hsync_end;
	modeinfo->htotal = (u16) mode->htotal;
	modeinfo->vdisplay = (u16) mode->vdisplay;
	modeinfo->vsync_start = (u16) mode->vsync_start;
	modeinfo->vsync_end = (u16) mode->vsync_end;
	modeinfo->vtotal = (u16) mode->vtotal;
	modeinfo->hskew = (u16) mode->hskew;
	modeinfo->vscan = (u16) mode->vscan;
	modeinfo->vrefresh = (u32) mode->vrefresh;
	modeinfo->flags = mode->flags;
	modeinfo->type |= mode->type | DRM_MODE_TYPE_PREFERRED;
	strncpy(modeinfo->name, mode->name, DRM_DISPLAY_MODE_LEN);

	return modeinfo;
}

struct drm_display_mode *
_display_mode_from_modeinfo(struct drm_mode_modeinfo *in)
{
	struct drm_display_mode *out;
	if (!in) {
		pr_err("ADF: HDMI: %s invalid input\n", __func__);
		return NULL;
	}

	out = kzalloc(sizeof(struct drm_display_mode), GFP_KERNEL);
	if (!out) {
		pr_err("ADF: HDMI: %s OOM while mode conversion\n", __func__);
		return NULL;
	}

	out->clock = in->clock;
	out->hdisplay = in->hdisplay;
	out->hsync_start = in->hsync_start;
	out->hsync_end = in->hsync_end;
	out->htotal = in->htotal;
	out->hskew = in->hskew;
	out->vdisplay = in->vdisplay;
	out->vsync_start = in->vsync_start;
	out->vsync_end = in->vsync_end;
	out->vtotal = in->vtotal;
	out->vscan = in->vscan;
	out->vrefresh = in->vrefresh;
	out->flags = in->flags;
	out->type = in->type;
	strncpy(out->name, in->name, DRM_DISPLAY_MODE_LEN);
	out->name[DRM_DISPLAY_MODE_LEN-1] = 0;

	pr_info("ADF: HDMI: %s\n", __func__);
	return out;
}

static bool
_monitor_is_hdmi(struct edid *edid)
{
	return detect_hdmi_monitor(edid);
}

static bool
_monitor_quant_range_selectable(struct edid *edid)
{
	return rgb_quant_range_selectable(edid);
}

static struct drm_mode_modeinfo *
_monitor_preferred_mode(struct hdmi_monitor *monitor)
{
	struct drm_mode_modeinfo *pref_mode = NULL;
	struct hdmi_mode_info *t, *cur_mode;

	list_for_each_entry_safe(cur_mode, t, &monitor->probed_modes, head) {
		/* Todo: Look for first detailed mode also */
		if (cur_mode->drm_mode.type & DRM_MODE_TYPE_PREFERRED) {
			pref_mode = &cur_mode->drm_mode;
			pr_info("ADF:HDMI: %s prefrerred mode = %s",
				__func__, cur_mode->drm_mode.name);
			break;
		}
	}

	if (!pref_mode) {
		pr_info("ADF: HDMI: %s no prefrerred mode found, falling to %s\n",
				__func__, fallback_mode.name);
		pref_mode = _display_mode_to_modeinfo(&fallback_mode);
	}

	return pref_mode;
}

static bool
_monitor_get_modes(struct hdmi_monitor *monitor, struct edid *edid)
{
	/*
	 * pase edid function:
	 * loads monitor modes and eld
	 * loads screen width/height
	 * sets quant range scalable flag
	 */
	monitor->no_probed_modes = _parse_edid(monitor, edid);
	if ((monitor->no_probed_modes <= 0) ||
			(list_empty(&monitor->probed_modes))) {
		pr_err("ADF: HDMI: %s No mode found in EDID\n", __func__);
		return false;
	}

	pr_info("ADF: HDMI: %s\n", __func__);
	return true;
}

static bool
_monitor_filter_modes(struct hdmi_monitor *monitor)
{
	struct drm_mode_modeinfo *mode;
	struct hdmi_mode_info *modeinfo, *t;
	list_for_each_entry_safe(modeinfo, t, &monitor->probed_modes, head) {
		mode = &modeinfo->drm_mode;
		if (!intel_adf_hdmi_mode_valid(mode)) {
			list_del(&modeinfo->head);
			pr_debug("ADF: HDMI: Dropping invalid mode %s\n",
				mode->name);
			if (!--monitor->no_probed_modes)
				break;
		}
	}

	pr_info("ADF: HDMI: %s\n", __func__);
	return true;
}

static bool
_monitor_load_fallback_modes(struct hdmi_monitor *monitor)
{

	INIT_LIST_HEAD(&fallback_mode.head);
	list_add(&monitor->probed_modes, &fallback_mode.head);
	pr_info("ADF: HDMI: %s\n", __func__);
	return true;
}

int intel_hdmi_self_modeset(struct hdmi_pipe *hdmi_pipe)
{
	int ret = 0;
	struct intel_pipe *pipe = &hdmi_pipe->base;
	struct hdmi_monitor *monitor = hdmi_pipe->config.ctx.monitor;
	struct drm_mode_modeinfo *mode;

	if (!monitor) {
		pr_err("ADF: HDMI: %s Monitor not available\n", __func__);
		return -EINVAL;
	}

	mode = monitor->preferred_mode;
	if (!mode) {
		pr_err("ADF: HDMI: %s No preferred mode\n", __func__);
		return -EINVAL;
	}

	pr_info("ADF: HDMI: %s Triggering self modeset on mode %s",
		__func__, mode->name);
	if (pipe->ops->modeset) {
		ret = pipe->ops->modeset(pipe, mode);
		if (ret) {
			pr_err("ADF: HDMI: %s Self modeset failed\n", __func__);
			return ret;
		}
		pr_info("ADF: HDMI: %s Self modeset success\n", __func__);
	} else {
		pr_err("ADF: HDMI: %s No modeset ??\n", __func__);
		return -EINVAL;
	}
	return ret;
}

bool intel_adf_hdmi_live_status(struct vlv_hdmi_port *hdmi_port)
{
	u32 detect = CHV_HPD_LIVE_STATUS(hdmi_port->port_id);
	if (REG_READ(hdmi_port->hpd_detect) & detect)
		return true;
	else
		return false;
}

struct edid *intel_adf_hdmi_get_edid(struct vlv_hdmi_port *hdmi_port)
{
	u8 retry = 3;
	struct edid *edid = NULL;

	/*
	 * Verify this:
	 * We are here means HDMI connection has been verified.
	 * Few HDMI cables / monitors are shy to share EDID. Adding some
	 * random retry and delay for them.
	 */
	while (retry) {
		edid = get_edid(hdmi_port->adapter);
		if (edid) {
			pr_info("ADF: HDMI: %s got EDID\n", __func__);
			break;
		} else {
			retry--;
			mdelay(2);
		}
	}

	return edid;
}

/* Must free the return ptr */
struct hdmi_monitor *
intel_adf_hdmi_get_monitor(struct edid *edid)
{
	struct hdmi_monitor *monitor = NULL;

	if (!edid) {
		pr_err("ADF: HDMI: %s NULL edid\n", __func__);
		goto out;
	}

	/* Allocate a monitor and load it */
	monitor = kzalloc(sizeof(*monitor), GFP_KERNEL);
	if (!monitor) {
		pr_err("ADF: HDMI: %s OOO\n", __func__);
		goto out;
	}

	INIT_LIST_HEAD(&monitor->probed_modes);
	monitor->is_hdmi = _monitor_is_hdmi(edid);
	monitor->quant_range_selectable = _monitor_quant_range_selectable(edid);

	/* Get all the modes supported in EDID */
	if (!_monitor_get_modes(monitor, edid)) {
		pr_err("ADF: HDMI: %s Cant read modes from EDID\n", __func__);
		goto fallback;
	}

	/* Drop modes not supported by SOC */
	if (!_monitor_filter_modes(monitor)) {
		pr_err("ADF: HDMI: %s Cant apply filter for modes\n", __func__);
		goto fallback;
	}

	monitor->preferred_mode = _monitor_preferred_mode(monitor);
	if (!monitor->preferred_mode)
		pr_err("ADF: HDMI: %s No preferred mode\n", __func__);
	pr_info("ADF: HDMI: %s: Monitor loaded\n", __func__);
out:
	return monitor;

fallback:
	if (!_monitor_load_fallback_modes(monitor)) {
		pr_err("ADF: HDMI: %s fallbak failed\n", __func__);
		kfree(monitor);
		monitor = NULL;
	}
	return monitor;
}

int intel_adf_hdmi_probe(struct hdmi_pipe *hdmi_pipe, bool force)
{
	bool previous_state;
	bool live_status = false;

	int ret;
	int retry = 10;
	struct edid *edid = NULL;
	struct hdmi_monitor *monitor = NULL;
	struct hdmi_context *hdmi_ctx = &hdmi_pipe->config.ctx;
	struct vlv_hdmi_port *hdmi_port;

	if (!hdmi_pipe) {
		pr_err("ADF: HDMI: %s No HDMI pipe\n", __func__);
		return -EINVAL;
	}

	hdmi_port = _hdmi_port_from_pipe(hdmi_pipe);
	if (!hdmi_port) {
		pr_err("ADF: HDMI: %s No HDMI port\n", __func__);
		return -EINVAL;
	}

	/*
	 * Live status can be down in case of a connected boots. Lets
	 * try to read EDID only if its bootup (force =1) time
	 */
	if (force) {
		pr_info("ADF: %s: force reading EDID\n", __func__);
		goto read_edid;
	}

	live_status = intel_adf_hdmi_live_status(hdmi_port);
	previous_state = atomic_read(&hdmi_ctx->connected);
	pr_info("ADF: HDMI: %s: live status=%d previous=%d\n",
		__func__, live_status, previous_state);

	if (!previous_state && (live_status == previous_state)) {
		/*
		 * We are here, means there can be an interrupt, so live
		 * status reg should reflect the opposite of previous
		 * state. But few HDMI monitors tend to set live status after
		 * some delay, so allow them if this can be a change from
		 * (disconnect->connect) by comparing with previous state.
		 * A delay of 2ms * 10 times = 20ms max
		 * Skip this loop only during boot time (force off)
		 */
		pr_info("ADF: HDMI: %s: status change\n", __func__);
		do {
			mdelay(2);
			live_status = intel_adf_hdmi_live_status(hdmi_port);
		} while (live_status == previous_state && --retry);
	}

read_edid:
	if (live_status != atomic_read(&hdmi_ctx->connected)
		|| force) {
		pr_info("ADF: HDMI: %s valid HPD(%s)\n", __func__,
			live_status ? "connected" : "disconnected");

		/*
		 * Try to read EDID only if live status allows. This helps
		 * when testing with HDMI analyzers, where the cable is
		 * always connected, and only HPD pin is toggled.
		 * This can cause a max delay of 10ms, if EDID is
		 * not readily available.
		 */
		if (live_status || force) {
			edid = intel_adf_hdmi_get_edid(hdmi_port);
			if (!edid) {
				pr_err("ADF: HDMI: %s Failed to read EDID\n",
					__func__);
#ifdef HDMI_USE_FALLBACK_EDID
				edid = kzalloc(EDID_LENGTH, GFP_KERNEL);
				if (!edid) {
					pr_err("ADF: HDMI: OOM(EDID)\n");
					return -ENOMEM;
				}

				/* Fallback to hard coded EDID */
				memcpy(edid, fallback_edid, EDID_LENGTH);
				pr_info("ADF: HDMI: falback EDID loaded\n");
				live_status = true;
#else
				live_status = false;
#endif
			} else {
				monitor = intel_adf_hdmi_get_monitor(edid);
				if (!monitor) {
					pr_err("ADF: HDMI: %s cant load monitor\n",
						__func__);
					return -EFAULT;
				}
				pr_info("\nADF: HDMI: %s HDMI Connected\n",
						__func__);
				live_status = true;
			}
		} else {
			pr_info("\nADF: HDMI: %s HDMI Disconnected\n",
				__func__);
		}

		/* Update live status */
		atomic_set(&hdmi_ctx->connected, live_status);

		/* Release old ptrs (kfree is NULL protected) */
		if (hdmi_ctx->monitor) {
			kfree(hdmi_ctx->monitor->edid);
			kfree(hdmi_ctx->monitor);
		}

		/* Update new monitor */
		if (monitor)
			monitor->edid = edid;
		hdmi_ctx->monitor = monitor;

		/* Notify audio about hotplug */
		ret = intel_adf_hdmi_notify_audio(hdmi_pipe, live_status);
		if (ret) {
			pr_err("ADF: HDMI: %s Noti to audio failed\n",
				__func__);
			return ret;
		}
	} else {
		pr_info("ADF: HDMI: %s no hotplug (live status =saved state)\n",
			__func__);
	}

	pr_info("ADF: HDMI: %s\n", __func__);
	return 0;
}


int intel_adf_hdmi_hot_plug(struct hdmi_pipe *hdmi_pipe)
{
	int ret = 0;
	struct hdmi_context *hdmi_ctx;

	/* Check HDMI status */
	ret = intel_adf_hdmi_probe(hdmi_pipe, false);
	if (ret) {
		pr_err("ADF: HDMI: %s Probing HDMI failed\n", __func__);
		return ret;
	}

	pr_info("ADF: HDMI: %s: probed\n", __func__);

	/* Trigger modeset if HDMI connected */
	hdmi_ctx = &hdmi_pipe->config.ctx;
	if (atomic_read(&hdmi_ctx->connected)) {
		pr_info("ADF: HDMI: %s: Triggering self modeset\n", __func__);
		ret = intel_hdmi_self_modeset(hdmi_pipe);
		if (ret) {
			pr_err("ADF: HDMI: %s Modeset failed\n", __func__);
			return ret;
		}
	} else {
		/* Todo: Disable HDMI */
		pr_info("ADF: HDMI: %s HDMI disabled\n", __func__);
	}
	pr_info("ADF: HDMI: %s\n", __func__);
	return 0;
}

int intel_adf_hdmi_get_events(struct hdmi_pipe *hdmi_pipe, u32 *events)
{
	return 0;
}

int intel_adf_hdmi_set_events(struct hdmi_pipe *hdmi_pipe,
		u32 events, bool enabled)
{
	return 0;
}

bool intel_adf_hdmi_get_hw_events(struct hdmi_pipe *hdmi_pipe)
{
	return true;
}


int intel_adf_hdmi_handle_events(struct hdmi_pipe *hdmi_pipe, u32 events)
{
	bool live_status = false;
	struct hdmi_context *hdmi_ctx;
	struct vlv_hdmi_port *hdmi_port;
	struct workqueue_struct *hotplug_wq;
	struct work_struct *hotplug_work;

	pr_debug("ADF: HDMI: %s\n", __func__);

	/* HDMI expects hot plug event */
	if (events & INTEL_PORT_EVENT_HOTPLUG_DISPLAY) {

		/* Validate input */
		if (!g_adf_context) {
			pr_err("ADF: %s: No adf context present\n", __func__);
			return -EINVAL;
		}

		hotplug_wq = g_adf_context->hotplug_wq;
		hotplug_work = (struct work_struct *)
				&g_adf_context->hotplug_work;

		if (!hdmi_pipe) {
			pr_err("ADF: %s: Not a valid HDMI pipe\n", __func__);
			return -EINVAL;
		}

		hdmi_ctx = &hdmi_pipe->config.ctx;
		hdmi_port = _hdmi_port_from_pipe(hdmi_pipe);

		/*
		 * Verify this: live_status reg in previous HWs were not
		 * stable and used to take time to detect the actual
		 * HPD status. Think about a delay(not sleep) here if reqd.
		 */
		live_status = intel_adf_hdmi_live_status(hdmi_port);
		if (live_status != atomic_read(&hdmi_ctx->connected)) {

			/* Queue bottom half */
			hdmi_ctx->top_half_status = true;
			queue_work(hotplug_wq, hotplug_work);
			pr_info("ADF: %s: HDMI state change detected (%d to %d)\n",
				__func__, atomic_read(&hdmi_ctx->connected),
						live_status);
		} else {
			pr_info("ADF: %s: No HDMI state change(%d)\n",
				__func__, live_status);
		}
	}

	return 0;
}

bool hdmi_notify_audio(struct hdmi_pipe *hdmi_pipe, bool connected)
{
	pr_info("ADF: HDMI: %s FIXME\n", __func__);
	return false;
}

int intel_adf_hdmi_notify_audio(struct hdmi_pipe *hdmi_pipe, bool connected)
{
	return hdmi_notify_audio(hdmi_pipe, connected);
}

int intel_hdmi_short_edid_read(struct i2c_adapter *adapter)
{
	return 0;
}
