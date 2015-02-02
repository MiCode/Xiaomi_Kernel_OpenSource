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

#ifndef GEN_HDMI_PIPE_H_
#define GEN_HDMI_PIPE_H_

#include "core/intel_dc_config.h"
#include "core/common/hdmi/gen_hdmi_audio.h"
#include <linux/extcon.h>
#include <linux/types.h>

#define HDMI_MAX_ELD_LENGTH	84
#define HDMI_DIP_PACKET_HEADER_LEN	3
#define HDMI_DIP_PACKET_DATA_LEN	28
#define CHV_HDMI_MAX_CLK_KHZ 297000
#define CHV_HDMI_MIN_CLK_KHZ	20000
#define VSYNC_COUNT_MAX_MASK 0xffffff

struct  avi_info_packet {
	uint8_t header[HDMI_DIP_PACKET_HEADER_LEN];
	union {
		uint8_t data[HDMI_DIP_PACKET_DATA_LEN];
		uint32_t data32[HDMI_DIP_PACKET_DATA_LEN/4];
	};
};

struct hdmi_mode_info {
	struct list_head head;
	struct drm_mode_modeinfo drm_mode;
	int mode_status;
};

struct hdmi_hotplug_context {
	atomic_t is_asserted;
	atomic_t is_connected;
};

struct  hdmi_avi_info_packet {
	uint8_t header[HDMI_DIP_PACKET_HEADER_LEN];
	union {
		uint8_t data[HDMI_DIP_PACKET_DATA_LEN];
		uint32_t data32[HDMI_DIP_PACKET_DATA_LEN/4];
	};
};

struct hdmi_monitor {
	struct edid *edid;

	uint8_t eld[HDMI_MAX_ELD_LENGTH];

	/* information parsed from edid*/
	bool is_hdmi;
	bool has_audio;

	struct list_head probed_modes;
	struct drm_mode_modeinfo *preferred_mode;
	struct avi_info_packet avi_packet;

	int screen_width_mm;
	int screen_height_mm;
	bool quant_range_selectable;
	u8 video_code;
	u8 no_probed_modes;
};

struct hdmi_audio {
	bool status;
	uint8_t eld[HDMI_MAX_ELD_LENGTH];
};

struct hdmi_context {
	atomic_t connected;
	u32 top_half_status;
	struct hdmi_monitor *monitor;
	struct hdmi_audio audio;
	u32 pixel_format;
	struct drm_mode_modeinfo *current_mode;
};

struct hdmi_config {
	struct hdmi_context ctx;
	struct mutex ctx_lock;
	struct drm_display_mode *force_mode;

	int pipe;
	int changed;
	int bpp;
	int ddc_bus;
	u32 pixel_multiplier;
	u8 lut_r[256], lut_g[256], lut_b[256];
};

struct hdmi_port;
struct hdmi_port_ops {
	u32 (*enable)(struct hdmi_port *port, u32 *port_bits);
	u32 (*disable)(struct hdmi_port *port,
		struct hdmi_config *config);
	u32 (*prepare)(struct hdmi_port *port,
		struct drm_mode_modeinfo *mode,
			struct hdmi_config *config);
};

struct hdmi_port {
	struct hdmi_port_ops ops;
	enum port idx;
	struct i2c_adapter *adapter;
	u32 control_reg;
	u32 hpd_status;
	u32 audio_ctrl;
	u32 dip_stat;
	u32 dip_ctrl;
	u32 dip_data;
	u32 hpd_detect;
	u32 hpd_ctrl;
};

struct hdmi_pipe {
	u8 dpms_state;
	struct hdmi_pipe_ops {
		int (*set_event)(struct hdmi_pipe *, u32, bool);
		int (*get_events)(struct hdmi_pipe *, u32 *);
		int (*handle_events)(struct hdmi_pipe *, u32);
		bool (*get_hw_state)(struct hdmi_pipe *);
		void (*on_post)(struct hdmi_pipe *);
	} ops;
	struct intel_pipe base;
	struct hdmi_config config;
	struct hdmi_port *port;
	struct work_struct hotplug_work;

	/* Added for HDMI audio */
	uint32_t tmds_clock;
#ifdef CONFIG_EXTCON
	struct extcon_dev hotplug_switch;
#endif
};

static inline struct hdmi_pipe *
hdmi_pipe_from_intel_pipe(struct intel_pipe *pipe)
{
	return container_of(pipe, struct hdmi_pipe, base);
}

static inline struct hdmi_pipe *
hdmi_pipe_from_cfg(struct hdmi_config *config)
{
	return container_of(config, struct hdmi_pipe, config);
}

static inline struct hdmi_config *
hdmi_config_from_ctx(struct hdmi_context *context)
{
	return container_of(context, struct hdmi_config, ctx);
}

/* gen_hdmi_pipe_ops.c */
extern struct intel_pipe_ops hdmi_base_ops;

/* i915_adf_wrapper.c */
extern struct i2c_adapter *intel_adf_get_gmbus_adapter(u8 port);

/* gen_hdmi_hotplug.c */
extern int
intel_adf_hdmi_hot_plug(struct hdmi_pipe *hdmi_pipe);
extern int
intel_adf_hdmi_notify_audio(struct hdmi_pipe *hdmi_pipe, bool connected);
extern int
intel_adf_hdmi_probe(struct hdmi_pipe *hdmi_pipe, bool force);
extern int
intel_hdmi_self_modeset(struct hdmi_pipe *hdmi_pipe);
extern int
intel_adf_hdmi_handle_events(struct hdmi_pipe *hdmi_pipe, u32 events);
extern bool
intel_adf_hdmi_get_hw_events(struct hdmi_pipe *hdmi_pipe);
extern int
intel_adf_hdmi_get_events(struct hdmi_pipe *hdmi_pipe, u32 *events);
extern int
intel_adf_hdmi_set_events(struct hdmi_pipe *hdmi_pipe,
			u32 events, bool enabled);
extern bool
intel_adf_hdmi_mode_valid(struct drm_mode_modeinfo *mode);
extern int
hdmi_pipe_init(struct hdmi_pipe *pipe, struct device *dev,
			struct intel_plane *primary_plane, u8 pipe_id,
				struct intel_pipeline *pipeline);
extern void
hdmi_pipe_destroy(struct hdmi_pipe *pipe);

/* Added for HDMI audio */
extern void adf_hdmi_audio_init(struct hdmi_pipe *hdmi_pipe);
extern void adf_hdmi_audio_signal_event(enum had_event_type event);
#endif /* GEN_HDMI_PIPE_H_ */
