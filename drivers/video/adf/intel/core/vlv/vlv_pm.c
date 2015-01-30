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

#include <intel_adf_device.h>
#include <core/vlv/vlv_dc_regs.h>
#include <core/vlv/vlv_dc_config.h>
#include <core/vlv/vlv_pm.h>

void vlv_pm_on_post(struct intel_dc_config *intel_config)
{
	struct vlv_dc_config *config = to_vlv_dc_config(intel_config);
	u32 pipe_plane_stat = config->status.pipe_plane_status;
	u32 plane_stat = pipe_plane_stat & 0x01FF;
	u32 val = 0;

	/* Enable maxfifo if required */
	if (!config->status.maxfifo_enabled &&
			((plane_stat == 1) ||
			(single_plane_enabled(plane_stat)))) {
		if  (intel_config->id == gen_cherryview) {
			/*
			 * In chv pipe-c should not be enabled for
			 * maxfifo to be enabled
			 */
			if (pipe_plane_stat & (1 << (31 - PIPE_C)))
				return;
		}
		REG_WRITE(FW_BLC_SELF_VLV, FW_CSPWRDWNEN);
		if  (intel_config->id == gen_cherryview) {
			val = vlv_punit_read(CHV_DPASSC);
			vlv_punit_write(CHV_DPASSC,
					(val | CHV_PW_MAXFIFO_MASK));
		}
		config->status.maxfifo_enabled = true;
	}
}

void vlv_pm_pre_validate(struct intel_dc_config *intel_config,
		struct intel_adf_post_custom_data *custom,
		struct intel_pipeline *intel_pipeline, struct intel_pipe *pipe)
{
	struct vlv_dc_config *config = to_vlv_dc_config(intel_config);
	struct vlv_pipeline *pipeline = to_vlv_pipeline(intel_pipeline);
	struct intel_adf_config *custom_config;
	u32 pipe_plane_stat = config->status.pipe_plane_status;
	u32 pipe_stat = pipe_plane_stat & 0xF0000000;
	u8 i = 0, planes_enabled = 0;
	u32 val = 0;

	for (i = 0; i < custom->n_configs; i++) {
		custom_config = &custom->configs[i];

		/* Get the number of planes enabled */
		if (custom_config->type == INTEL_ADF_CONFIG_PLANE)
			planes_enabled++;
	}

	/* If we are moving to multiple plane then disable maxfifo */
	if (((planes_enabled > 1) || !(single_pipe_enabled(pipe_stat))) &&
			config->status.maxfifo_enabled) {
		REG_WRITE(FW_BLC_SELF_VLV, ~FW_CSPWRDWNEN);
		if  (intel_config->id == gen_cherryview) {
			val = vlv_punit_read(CHV_DPASSC);
			vlv_punit_write(CHV_DPASSC,
					(val & ~CHV_PW_MAXFIFO_MASK));
		}
		/* FIXME: move these variables out of intel_pipe */
		config->status.maxfifo_enabled = false;
		pipeline->status.wait_vblank = true;
		pipeline->status.vsync_counter =
				pipe->ops->get_vsync_counter(pipe, 0);
	}
}

void vlv_pm_pre_post(struct intel_dc_config *intel_config,
		struct intel_pipeline *intel_pipeline, struct intel_pipe *pipe)
{
	struct vlv_pipeline *pipeline = to_vlv_pipeline(intel_pipeline);
	struct drm_mode_modeinfo mode;

	if (pipeline->status.wait_vblank && pipeline->status.vsync_counter ==
			pipe->ops->get_vsync_counter(pipe, 0)) {
		vlv_wait_for_vblank(intel_pipeline);
		pipeline->status.wait_vblank = false;
	}

	pipe->ops->get_current_mode(pipe, &mode);
	vlv_evade_vblank(intel_pipeline, &mode,
				&pipeline->status.wait_vblank);
}

bool vlv_calc_ddl(int clock, int pixel_size, int *prec_multi, int *ddl)
{
	int entries;
	bool latencyprogrammed = false;

	entries = DIV_ROUND_UP(clock, 1000) * pixel_size;
	*prec_multi = (entries > 256) ?
		DDL_PRECISION_H : DDL_PRECISION_L;
	*ddl = (64 * (*prec_multi) * 4) / entries;
	latencyprogrammed = true;

	/*
	 * chv: divide the calculated ddl by 2,
	 * for supporting PM5 and DDR DVFS
	 */
	if (IS_CHERRYVIEW())
		*ddl /= 2;

	return latencyprogrammed;
}

bool vlv_pm_update_maxfifo_status(struct vlv_pm *pm, bool enable)
{
	u32 val = 0;

	if (enable) {
		REG_WRITE(FW_BLC_SELF_VLV, FW_CSPWRDWNEN);
		if (IS_CHERRYVIEW()) {
			val = vlv_punit_read(CHV_DPASSC);
			vlv_punit_write(CHV_DPASSC,
					(val | CHV_PW_MAXFIFO_MASK));
		}
	} else {
		REG_WRITE(FW_BLC_SELF_VLV, ~FW_CSPWRDWNEN);
		if (IS_CHERRYVIEW()) {
			val = vlv_punit_read(CHV_DPASSC);
			vlv_punit_write(CHV_DPASSC,
					(val & ~CHV_PW_MAXFIFO_MASK));
		}
	}

	return true;
}

u32 vlv_pm_save_values(struct vlv_pm *pm, bool pri_plane,
		bool sp1_plane, bool sp2_plane, u32 val)
{
	u32 mask = 0;

	if (pri_plane) {
		pm->pri_value = val;
		mask = DDL_PLANEA_MASK;
	}

	if (sp1_plane) {
		pm->sp1_value = val;
		mask = DDL_SPRITEA_MASK;
	}

	if (sp2_plane) {
		pm->sp2_value = val;
		mask = DDL_SPRITEB_MASK;
	}

	REG_WRITE_BITS(pm->offset, 0, mask);
	return 0;
}

u32 vlv_pm_flush_values(struct vlv_pm *pm, u32 events)
{
	if (pm->sp2_value && (events & INTEL_PIPE_EVENT_SPRITE2_FLIP)) {
		REG_WRITE_BITS(pm->offset, pm->sp2_value, DDL_SPRITEB_MASK);
		pm->sp2_value = 0;
	}

	if (pm->sp1_value && (events & INTEL_PIPE_EVENT_SPRITE1_FLIP)) {
		REG_WRITE_BITS(pm->offset, pm->sp1_value, DDL_SPRITEA_MASK);
		pm->sp1_value = 0;
	}

	if (pm->pri_value && (events & INTEL_PIPE_EVENT_PRIMARY_FLIP)) {
		REG_WRITE_BITS(pm->offset, pm->pri_value, DDL_PLANEA_MASK);
		pm->pri_value = 0;
	}

	return 0;
}

static void vlv_pm_update_pfi(struct vlv_pm *pm)
{
	/* Trickle feed is disabled by default */
	REG_WRITE(MI_ARB_VLV, 0x00);
	/* program the pfi credits, first disable and then program */
	if (REG_READ(GCI_CONTROL) != 0x78004000) {
		REG_WRITE(GCI_CONTROL, 0x00004000);
		REG_WRITE(GCI_CONTROL, 0x78004000);
	}

}

u32 vlv_pm_program_values(struct vlv_pm *pm, int num_planes)
{
	/* FIXME: udpate logic to be based on num_planes enabled */
	REG_WRITE(DSPFW1,
		(DSPFW_SR_VAL << DSPFW_SR_SHIFT) |
		(DSPFW_CURSORB_VAL << DSPFW_CURSORB_SHIFT) |
		(DSPFW_PLANEB_VAL << DSPFW_PLANEB_SHIFT) |
		DSPFW_PLANEA_VAL);
	REG_WRITE(DSPFW2,
		(DSPFW2_RESERVED) |
		(DSPFW_CURSORA_VAL << DSPFW_CURSORA_SHIFT) |
		DSPFW_PLANEC_VAL);
	REG_WRITE(DSPFW3,
		(REG_READ(DSPFW3) & ~DSPFW_CURSOR_SR_MASK) |
		(DSPFW3_VLV));
	REG_WRITE(DSPFW4, (DSPFW4_SPRITEB_VAL << DSPFW4_SPRITEB_SHIFT) |
			(DSPFW4_CURSORA_VAL << DSPFW4_CURSORA_SHIFT) |
			DSPFW4_SPRITEA_VAL);
	REG_WRITE(DSPFW5, (DSPFW5_DISPLAYB_VAL << DSPFW5_DISPLAYB_SHIFT) |
			(DSPFW5_DISPLAYA_VAL << DSPFW5_DISPLAYA_SHIFT) |
			(DSPFW5_CURSORB_VAL << DSPFW5_CURSORB_SHIFT) |
			DSPFW5_CURSORSR_VAL);
	REG_WRITE(DSPFW6, DSPFW6_DISPLAYSR_VAL);
	REG_WRITE(DSPFW7, (DSPFW7_SPRITED1_VAL << DSPFW7_SPRITED1_SHIFT) |
			(DSPFW7_SPRITED_VAL << DSPFW7_SPRITED_SHIFT) |
			(DSPFW7_SPRITEC1_VAL << DSPFW7_SPRITEC1_SHIFT) |
			DSPFW7_SPRITEC_VAL);
	REG_WRITE(DSPARB, VLV_DEFAULT_DSPARB);

	vlv_pm_update_pfi(pm);
	return 0;
}

bool vlv_pm_init(struct vlv_pm *pm, enum pipe pipe)
{
	pm->offset = VLV_DDL(pipe);

	return true;
}

bool vlv_pm_destroy(struct vlv_pm *pm)
{
	return true;
}
