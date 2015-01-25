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
 */

#ifndef VLV_DC_CONFIG_H
#define VLV_DC_CONFIG_H

#include "core/intel_dc_config.h"
#include "core/vlv/vlv_dc_regs.h"
#include <drm/i915_drm.h>
#include <drm/i915_adf.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_pri_plane.h>
#include <core/vlv/vlv_sp_plane.h>
#include <core/vlv/vlv_dpst.h>
#include <core/vlv/vlv_dsi_port.h>
#include <core/vlv/vlv_pll.h>
#include <core/vlv/vlv_pipe.h>
#include <core/vlv/vlv_pm.h>

#define VLV_N_PLANES	6
#define VLV_N_PIPES	2

#define CHV_N_PLANES	9
#define CHV_N_PIPES	3

#define VLV_NUM_SPRITES 2
#define VLV_SP_12BIT_MASK 0xFFF

enum planes {
	PRIMARY_PLANE = 0,
	SPRITE_A = 1,
	SPRITE_B = 2,
	SECONDARY_PLANE = 3,
	SPRITE_C = 4,
	SPRITE_D = 5,
	TERTIARY_PLANE = 6,
	SPRITE_E = 7,
	SPRITE_F = 8,
	NUM_PLANES,
};

enum vlv_disp_plane {
	VLV_PLANE = 0,
	VLV_SPRITE1,
	VLV_SPRITE2,
	VLV_MAX_PLANES,
};

static inline void vlv_gpio_write(u32 port, u32 reg, u32 val)
{
	intel_adf_pci_sideband_rw(INTEL_SIDEBAND_REG_WRITE, port, reg, &val);
}

static inline void vlv_flisdsi_write(u32 reg, u32 val)
{
	intel_adf_dpio_sideband_rw(INTEL_SIDEBAND_REG_WRITE, IOSF_PORT_FLISDSI,
				reg, &val);
}

static inline void vlv_gpio_nc_write(u32 reg, u32 val)
{
	intel_adf_dpio_sideband_rw(INTEL_SIDEBAND_REG_WRITE,
					IOSF_PORT_GPIO_NC, reg, &val);
}

static inline u32 vlv_gps_core_read(u32 reg)
{
	u32 val;
	intel_adf_pci_sideband_rw(INTEL_SIDEBAND_REG_READ,
					IOSF_PORT_GPS_CORE, reg, &val);
	return val;
}

static inline void vlv_gps_core_write(u32 reg, u32 val)
{
	intel_adf_pci_sideband_rw(INTEL_SIDEBAND_REG_WRITE,
					IOSF_PORT_GPS_CORE, reg, &val);
}

static inline u32 vlv_cck_read(u32 reg)
{
	u32 val;
	intel_adf_pci_sideband_rw(INTEL_SIDEBAND_REG_READ, IOSF_PORT_CCK,
					reg, &val);
	return val;
}

static inline void vlv_cck_write(u32 reg, u32 val)
{
	intel_adf_pci_sideband_rw(INTEL_SIDEBAND_REG_WRITE, IOSF_PORT_CCK,
					reg, &val);
}
struct vlv_pipeline {
	struct intel_pipeline base;
	struct vlv_dpst *dpst;
	struct vlv_pll pll;
	struct vlv_pm pm;
	struct vlv_pipe pipe;
	struct vlv_pri_plane pplane;
	struct vlv_sp_plane splane[2];
	enum intel_pipe_type type;
	union {
		struct dsi_pipe dsi;
		/* later we will have hdmi pipe */
	} gen;
	union {
		struct vlv_dsi_port dsi_port;
		/* later we will have hdmi port */
	} port;

};

struct vlv_dc_config {
	struct intel_dc_config base;
	u32 max_pipes;
	u32 max_planes;
	struct vlv_dpst dpst;
	/*
	 * FIXME: For other platforms the number of pipes may
	 * vary, which has to be handeled in future
	 */
	struct vlv_pipeline pipeline[CHV_N_PIPES];
};

static inline struct vlv_dc_config *to_vlv_dc_config(
	struct intel_dc_config *config)
{
	return container_of(config, struct vlv_dc_config, base);
}

static inline struct vlv_pipeline *to_vlv_pipeline(
	struct intel_pipeline *intel_pipeline)
{
	return container_of(intel_pipeline, struct vlv_pipeline, base);
}

bool vlv_intf_screen_connected(struct intel_pipeline *pipeline);
u32 vlv_intf_vsync_counter(struct intel_pipeline *pipeline, u32 interval);

/* vlv_modeset */
extern enum port vlv_get_connected_port(struct intel_pipe *intel_pipe);
extern bool vlv_wait_for_vblank(struct intel_pipeline *pipeline);
extern void vlv_evade_vblank(struct intel_pipeline *pipeline,
			struct drm_mode_modeinfo *mode, bool *wait_for_vblank);
extern u32 vlv_dsi_prepare_on(struct intel_pipeline *pipeline,
			struct drm_mode_modeinfo *mode);
extern u32 vlv_dsi_pre_pipeline_on(struct intel_pipeline *pipeline,
			struct drm_mode_modeinfo *mode);
extern u32 vlv_pipeline_on(struct intel_pipeline *pipeline,
			struct drm_mode_modeinfo *mode);
extern u32 vlv_pipeline_off(struct intel_pipeline *pipeline);
extern u32 vlv_post_pipeline_off(struct intel_pipeline *pipeline);
extern bool vlv_is_screen_connected(struct intel_pipeline *pipeline);
extern u32 vlv_num_planes_enabled(struct intel_pipeline *pipeline);
extern bool vlv_is_vid_mode(struct intel_pipeline *pipeline);
extern bool vlv_can_be_disabled(struct intel_pipeline *pipeline);
extern bool vlv_update_maxfifo_status(struct intel_pipeline *pipeline,
		bool enable);
extern int vlv_cmd_dpi_send_cmd(struct intel_pipeline *pipeline, u32 cmd,
		bool hs);
extern int vlv_cmd_vc_generic_read(struct intel_pipeline *pipeline,
		int channel, u8 *reqdata, int reqlen, u8 *buf, int buflen);
extern int vlv_cmd_vc_dcs_read(struct intel_pipeline *pipeline, int channel,
		u8 dcs_cmd, u8 *buf, int buflen);
extern int vlv_cmd_vc_generic_write(struct intel_pipeline *pipeline,
		int channel, const u8 *data, int len);
extern int vlv_cmd_vc_dcs_write(struct intel_pipeline *pipeline, int channel,
		const u8 *data, int len);
extern void vlv_cmd_hs_mode_enable(struct intel_pipeline *pipeline,
		bool enable);

extern bool vlv_is_plane_enabled(struct intel_pipeline *pipeline,
		struct intel_plane *plane);

/* vlv_dpst */
extern u32 vlv_dpst_context(struct intel_pipeline *pipeline,
		unsigned long args);
extern u32 vlv_dpst_irq_handler(struct intel_pipeline *pipeline);
/* vlv_debugfs */
extern int vlv_debugfs_init(struct vlv_dc_config *vlv_config);
extern void vlv_debugfs_teardown(struct vlv_dc_config *vlv_config);

/* port export functions */
bool vlv_dsi_port_init(struct vlv_dsi_port *port, enum port, enum pipe);
bool vlv_dsi_port_destroy(struct vlv_dsi_port *port);

/* reg access */
extern u32 REG_READ(u32 reg);
extern u32 REG_POSTING_READ(u32 reg);
extern void REG_WRITE(u32 reg, u32 val);
extern void REG_WRITE_BITS(u32 reg, u32 val, u32 mask);


#endif
