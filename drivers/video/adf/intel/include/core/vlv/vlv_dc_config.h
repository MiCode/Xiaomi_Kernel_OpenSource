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
#include <linux/i2c.h>
#include <core/common/dsi/dsi_pipe.h>
#include <core/common/hdmi/gen_hdmi_pipe.h>
#include <core/common/dp/gen_dp_pipe.h>
#include <core/vlv/vlv_pri_plane.h>
#include <core/vlv/vlv_sp_plane.h>
#include <core/vlv/vlv_dpst.h>
#include <core/vlv/vlv_dsi_port.h>
#include <core/vlv/vlv_hdmi_port.h>
#include <core/vlv/vlv_dp_port.h>
#include <core/vlv/vlv_pll.h>
#include <core/vlv/vlv_pipe.h>
#include <core/vlv/vlv_pm.h>
#include <core/vlv/dpio.h>

#define VLV_N_PLANES	6
#define VLV_N_PIPES	2

#define CHV_N_PLANES	9
#define CHV_N_PIPES	3

#define VLV_NUM_SPRITES 2
#define VLV_SP_12BIT_MASK 0xFFF

#define CHV_PCI_MINOR_STEP_MASK		0x0C
#define CHV_PCI_MAJOR_STEP_MASK		0x30

#define CHV_PCI_MINOR_STEP_SHIFT	0x02
#define CHV_PCI_MAJOR_STEP_SHIFT	0x04

#define CHV_PCI_STEP_SEL_MASK		0x40
#define CHV_PCI_STEP_SEL_SHIFT		0x06
#define CHV_PCI_OVERFLOW_MASK		0x80
#define CHV_PCI_OVERFLOW_SHIFT		0x07

#define CHV_MAX_STEP_SEL		1
#define CHV_MAX_MAJ_STEP		1
#define CHV_MAX_MIN_STEP		3

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

static inline void vlv_dpio_write(u32 port, u32 reg, u32 val)
{
	intel_adf_dpio_sideband_rw(INTEL_SIDEBAND_REG_WRITE, port,
			reg, &val);
}

static inline u32 vlv_dpio_read(u32 port, u32 reg)
{
	u32 val;
	intel_adf_dpio_sideband_rw(INTEL_SIDEBAND_REG_READ, port,
			reg, &val);
	return val;
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

static inline u32 vlv_punit_read(u32 reg)
{
	u32 val;
	intel_adf_pci_sideband_rw(INTEL_SIDEBAND_REG_READ, IOSF_PORT_PUNIT,
			reg, &val);
	return val;
}

static inline void vlv_punit_write(u32 reg, u32 val)
{
	intel_adf_pci_sideband_rw(INTEL_SIDEBAND_REG_WRITE, IOSF_PORT_PUNIT,
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
	u32 dpio_id;
	u32 disp_no;
	enum intel_pipe_type type;
	u16 dc_stepping;
	union {
		struct dsi_pipe dsi;
		struct hdmi_pipe hdmi;
		struct dp_pipe dp;
	} gen;
	union {
		struct vlv_dsi_port dsi_port[ADF_MAX_PORTS - 1];
		struct vlv_hdmi_port hdmi_port;
		struct vlv_dp_port dp_port;
	} port;

};

struct vlv_dc_config {
	struct intel_dc_config base;
	u32 max_pipes;
	u32 max_planes;
	struct vlv_dpst dpst;
	struct mutex dpio_lock;
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
extern u32 chv_pipeline_off(struct intel_pipeline *pipeline);
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
		int channel, u8 *reqdata, int reqlen, u8 *buf, int buflen,
		enum port port);
extern int vlv_cmd_vc_dcs_read(struct intel_pipeline *pipeline, int channel,
		u8 dcs_cmd, u8 *buf, int buflen, enum port port);
extern int vlv_cmd_vc_generic_write(struct intel_pipeline *pipeline,
		int channel, const u8 *data, int len, enum port port);
extern int vlv_cmd_vc_dcs_write(struct intel_pipeline *pipeline, int channel,
		const u8 *data, int len, enum port port);
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

extern u32 vlv_aux_transfer(struct intel_pipeline *pipeline,
	struct dp_aux_msg *msg);
extern u32 vlv_set_signal_levels(struct intel_pipeline *pipeline,
	struct link_params *params);
extern u32 chv_set_signal_levels(struct intel_pipeline *pipeline,
	struct link_params *params);
extern u32 vlv_set_link_pattern(struct intel_pipeline *pipeline,
	u8 train_pattern);
extern void vlv_get_max_vswing_preemp(struct intel_pipeline *pipeline,
	enum vswing_level *max_v, enum preemp_level *max_p);
extern void vlv_get_adjust_train(struct intel_pipeline *pipeline,
	struct link_params *params);
extern u32 vlv_dp_panel_power_seq(struct intel_pipeline *pipeline,
	bool enable);
extern u32 vlv_dp_backlight_seq(struct intel_pipeline *pipeline,
	bool enable);
extern struct i2c_adapter *vlv_get_i2c_adapter(struct intel_pipeline *pipeline);

/* reg access */
extern u32 REG_READ(u32 reg);
extern u32 REG_POSTING_READ(u32 reg);
extern void REG_WRITE(u32 reg, u32 val);
extern void REG_WRITE_BITS(u32 reg, u32 val, u32 mask);

extern u32 vlv_dp_set_brightness(struct intel_pipeline *pipeline, int level);
extern u32 vlv_dp_get_brightness(struct intel_pipeline *pipeline);

#endif
