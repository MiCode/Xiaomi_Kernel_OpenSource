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

#ifndef INTEL_PLATFORM_CONFIG_H_
#define INTEL_PLATFORM_CONFIG_H_

#include <linux/kernel.h>
#include <linux/module.h>
#include <video/intel_adf.h>
#include <core/intel_dc_config.h>

#if defined(CONFIG_ADF)
#include <video/adf.h>
#endif

bool vlv_wait_for_vblank(struct intel_pipeline *pipeline);
void vlv_evade_vblank(struct intel_pipeline *pipeline,
		struct drm_mode_modeinfo *mode, bool *wait_for_vblank);
u32 vlv_dsi_prepare_on(struct intel_pipeline *pipeline,
		struct drm_mode_modeinfo *mode);
u32 vlv_dsi_pre_pipeline_on(struct intel_pipeline *pipeline,
		struct drm_mode_modeinfo *mode);
u32 vlv_pipeline_on(struct intel_pipeline *pipeline,
		struct drm_mode_modeinfo *mode);
u32 vlv_pipeline_off(struct intel_pipeline *pipeline);
u32 vlv_post_pipeline_off(struct intel_pipeline *pipeline);
bool vlv_is_screen_connected(struct intel_pipeline *pipeline);
u32 vlv_dpst_context(struct intel_pipeline *pipeline, unsigned long args);
u32 vlv_dpst_irq_handler(struct intel_pipeline *pipeline);
u32 vlv_num_planes_enabled(struct intel_pipeline *pipeline);
bool vlv_can_be_disabled(struct intel_pipeline *pipeline);
bool vlv_update_maxfifo_status(struct intel_pipeline *pipeline, bool enable);
int vlv_enable_plane(struct intel_pipeline *pipeline,
		struct intel_plane *plane);
int vlv_disable_plane(struct intel_pipeline *pipeline,
		struct intel_plane *plane);
bool vlv_is_plane_enabled(struct intel_pipeline *pipeline,
		struct intel_plane *plane);
void vlv_flip(struct intel_pipeline *pipeline,
		struct intel_plane *plane,
		struct intel_buffer *buf,
		struct intel_plane_config *config);
int vlv_validate(struct intel_pipeline *pipeline,
		struct intel_plane *plane,
		struct intel_buffer *buf,
		struct intel_plane_config *config);
int vlv_validate_custom_format(struct intel_pipeline *pipeline,
		struct intel_plane *plane, u32 format, u32 w, u32 h);
bool vlv_is_vid_mode(struct intel_pipeline *pipeline);
void vlv_cmd_hs_mode_enable(struct intel_pipeline *pipeline, bool enable);
int vlv_cmd_vc_dcs_write(struct intel_pipeline *pipeline, int channel,
		const u8 *data, int len);
int vlv_cmd_vc_generic_write(struct intel_pipeline *pipeline, int channel,
		const u8 *data, int len);
int vlv_cmd_vc_dcs_read(struct intel_pipeline *pipeline, int channel,
		u8 dcs_cmd, u8 *buf, int buflen);
int vlv_cmd_vc_generic_read(struct intel_pipeline *pipeline, int channel,
		u8 *reqdata, int reqlen, u8 *buf, int buflen);
int vlv_cmd_dpi_send_cmd(struct intel_pipeline *pipeline, u32 cmd, bool hs);

/*
 * Supported configs can be declared here for use inside
 * intel_adf_get_dc_config
 */


extern struct intel_dc_config *vlv_get_dc_config(struct pci_dev *pdev, u32 id);
extern void vlv_dc_config_destroy(struct intel_dc_config *config);

#endif /* INTEL_PLATFORM_CONFIG_H_ */
