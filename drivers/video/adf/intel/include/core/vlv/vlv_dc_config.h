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

#define VLV_N_PLANES	6
#define VLV_N_PIPES	2
#define VLV_NUM_SPRITES 2
#define VLV_SP_12BIT_MASK 0xFFF

enum pipe {
	PIPE_A = 0,
	PIPE_B,
	MAX_PIPES,
};

enum planes {
	PRIMARY_PLANE = 0,
	SPRITE_A = 1,
	SPRITE_B = 2,
	SECONDARY_PLANE = 3,
	SPRITE_C = 4,
	SPRITE_D = 5,
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

struct vlv_dc_config {
	struct intel_dc_config base;
	struct vlv_dpst dpst;
	struct vlv_disp {
		struct vlv_pri_plane pplane;
		struct vlv_sp_plane splane[2];
		enum intel_pipe_type type;
		union {
			struct dsi_pipe dsi;

			/* later we will have hdmi pipe */
		} pipe;

	} vdisp[2];
};

static inline struct vlv_dc_config *to_vlv_dc_config(
	struct intel_dc_config *config)
{
	return container_of(config, struct vlv_dc_config, base);
}

bool vlv_intf_screen_connected(struct intel_pipe *pipe);
u32 vlv_intf_vsync_counter(struct intel_pipe *pipe, u32 interval);
extern int pipe_mode_set(struct intel_pipe *pipe,
			 struct drm_mode_modeinfo *mode);
extern int vlv_display_on(struct intel_pipe *pipe);
extern int vlv_display_off(struct intel_pipe *pipe);
#endif
