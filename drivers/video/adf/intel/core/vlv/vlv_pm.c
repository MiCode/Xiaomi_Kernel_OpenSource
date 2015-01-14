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

bool vlv_pm_update_maxfifo_status(struct vlv_pm *pm, bool enable)
{
	if (enable)
		REG_WRITE(FW_BLC_SELF_VLV, FW_CSPWRDWNEN);
	else
		REG_WRITE(FW_BLC_SELF_VLV, ~FW_CSPWRDWNEN);

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
