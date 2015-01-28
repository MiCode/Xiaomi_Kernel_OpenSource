/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/

#ifndef DSI_PIPE_H_
#define DSI_PIPE_H_

#include "core/intel_dc_config.h"
#include "core/common/dsi/dsi_config.h"
#include "core/common/dsi/dsi_panel.h"
#ifndef CONFIG_ADF_INTEL_VLV
#include "pwr_mgmt.h"
#include "core/common/dsi/dsi_pkg_sender.h"
#endif
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
#include "core/common/backlight_dev.h"
#endif

#define for_each_dsi_port(__port, __ports_mask)  \
	for ((__port) = PORT_A; (__port) < ADF_MAX_PORTS; (__port)++) \
		if ((__ports_mask) & (1 << (__port)))


struct dsi_pipe;

/**
 * dsi_pipe_ops - required operations for a DSI pipe
 * @power_on: [required]
 * @power_off: [required]
 * @mode_set: [required]
 * @on_post: [optional]
 * @vsync: [required]
 */
struct dsi_pipe_ops {
	int (*power_on)(struct dsi_pipe *pipe);
	void (*pre_power_off)(struct dsi_pipe *pipe);
	int (*power_off)(struct dsi_pipe *pipe);
	int (*mode_set)(struct dsi_pipe *pipe,
		struct drm_mode_modeinfo *mode);
	void (*pre_post)(struct dsi_pipe *pipe);
	void (*on_post)(struct dsi_pipe *pipe);
	int (*set_event)(struct dsi_pipe *pipe, u8 event, bool enabled);
	void (*get_events)(struct dsi_pipe *pipe, u32 *events);
	void (*handle_events)(struct dsi_pipe *pipe, u32 events);
	void (*set_brightness)(u32 level);
	u32 (*get_brightness)(void);
	bool (*get_hw_state)(struct dsi_pipe *pipe);
};

struct dsi_pipe {
	struct intel_pipe base;
	struct dsi_pipe_ops ops;
	struct dsi_config config;
	struct intel_pipeline *pipeline;
#ifndef CONFIG_ADF_INTEL_VLV
	struct dsi_pkg_sender sender;
#endif
	struct dsi_panel *panel;
	u8 dpms_state;
};

static inline struct dsi_pipe *to_dsi_pipe(struct intel_pipe *pipe)
{
	return container_of(pipe, struct dsi_pipe, base);
}

#ifdef CONFIG_ADF_INTEL_VLV
extern int dsi_pipe_init(struct dsi_pipe *pipe, struct device *dev,
	struct intel_plane *primary_plane, u8 idx,
	struct intel_pipeline *pipeline, int port);
#else
extern int dsi_pipe_init(struct dsi_pipe *pipe, struct device *dev,
	struct intel_plane *primary_plane, u8 idx, u32 gtt_phy_addr);
#endif
extern void dsi_pipe_destroy(struct dsi_pipe *pipe);

extern bool dsi_pipe_enable_clocking(struct dsi_pipe *pipe);
extern bool dsi_pipe_disable_clocking(struct dsi_pipe *pipe);

#endif /* COMMON_DSI_PIPE_H_ */
