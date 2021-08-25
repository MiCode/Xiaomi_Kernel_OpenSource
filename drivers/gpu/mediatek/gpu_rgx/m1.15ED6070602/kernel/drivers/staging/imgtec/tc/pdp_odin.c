/*
 * @Codingstyle LinuxKernel
 * @Copyright   Copyright (c) Imagination Technologies Ltd. All Rights Reserved
 * @License     Dual MIT/GPLv2
 *
 * The contents of this file are subject to the MIT license as set out below.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 ("GPL") in which case the provisions
 * of GPL are applicable instead of those above.
 *
 * If you wish to allow use of your version of this file only under the terms of
 * GPL, and not to allow others to use your version of this file under the terms
 * of the MIT license, indicate your decision by deleting the provisions above
 * and replace them with the notice and other provisions required by GPL as set
 * out in the file called "GPL-COPYING" included in this distribution. If you do
 * not delete the provisions above, a recipient may use your version of this file
 * under the terms of either the MIT license or GPL.
 *
 * This License is also included in this distribution in the file called
 * "MIT-COPYING".
 *
 * EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
 * PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <linux/delay.h>
#include <linux/kernel.h>

#include "pdp_common.h"
#include "pdp_odin.h"
#include "odin_defs.h"
#include "odin_regs.h"
#include "orion_defs.h"
#include "orion_regs.h"
#include "pfim_defs.h"
#include "pfim_regs.h"

#define ODIN_PLL_REG(n)	((n) - ODN_PDP_P_CLK_OUT_DIVIDER_REG1)

struct odin_displaymode {
	int w;		/* display width */
	int h;		/* display height */
	int id;		/* pixel clock input divider */
	int m;		/* pixel clock multiplier */
	int od1;	/* pixel clock output divider */
	int od2;	/* mem clock output divider */
};

struct pfim_property {
	u32 tiles_per_line;
	u32 tile_type;
	u32 tile_xsize;
	u32 tile_ysize;
};

/*
 * For Odin, only the listed modes below are supported.
 * 1080p id=5, m=37, od1=5, od2=5
 * 720p id=5, m=37, od1=10, od2=5
 * 1280x1024 id=1, m=14, od1=13, od2=8
 * 1440x900 id=5, m=53, od1=10, od2=8
 * 1280x960 id=3, m=40, od1=13, od2=9
 * 1024x768 id=1, m=13, od1=20, od2=10
 * 800x600 id=2, m=20, od1=25, od2=7
 * 640x480 id=1, m=12, od1=48, od2=9
 * ... where id is the PDP_P_CLK input divider,
 * m is PDP_P_CLK multiplier regs 1 to 3
 * od1 is PDP_P_clk output divider regs 1 to 3
 * od2 is PDP_M_clk output divider regs 1 to 2
 */
static const struct odin_displaymode odin_modes[] = {
	{.w = 1920, .h = 1080, .id = 5, .m = 37, .od1 = 5, .od2 = 5},
	{.w = 1280, .h = 720, .id = 5, .m = 37, .od1 = 10, .od2 = 5},
	{.w = 1280, .h = 1024, .id = 1, .m = 14, .od1 = 13, .od2 = 10},
	{.w = 1440, .h = 900, .id = 5, .m = 53, .od1 = 10, .od2 = 8},
	{.w = 1280, .h = 960, .id = 3, .m = 40, .od1 = 13, .od2 = 9},
	{.w = 1024, .h = 768, .id = 1, .m = 13, .od1 = 20, .od2 = 10},
	{.w = 800, .h = 600, .id = 2, .m = 20, .od1 = 25, .od2 = 7},
	{.w = 640, .h = 480, .id = 1, .m = 12, .od1 = 48, .od2 = 9},
	{.w = 0, .h = 0, .id = 0, .m = 0, .od1 = 0, .od2 = 0}
};

/*
 * For Orion, only the listed modes below are supported.
 * 1920x1080 mode is currently not supported.
 */
static const struct odin_displaymode orion_modes[] = {
	{.w = 1280, .h = 720, .id = 5, .m = 37, .od1 = 10, .od2 = 7},
	{.w = 1280, .h = 1024, .id = 1, .m = 12, .od1 = 11, .od2 = 10},
	{.w = 1440, .h = 900, .id = 5, .m = 53, .od1 = 10, .od2 = 9},
	{.w = 1280, .h = 960, .id = 5, .m = 51, .od1 = 10, .od2 = 9},
	{.w = 1024, .h = 768, .id = 3, .m = 33, .od1 = 17, .od2 = 10},
	{.w = 800, .h = 600, .id = 2, .m = 24, .od1 = 31, .od2 = 12},
	{.w = 640, .h = 480, .id = 1, .m = 12, .od1 = 50, .od2 = 12},
	{.w = 0, .h = 0, .id = 0, .m = 0, .od1 = 0, .od2 = 0}
};

static const struct pfim_property pfim_properties[] = {
	[ODIN_PFIM_MOD_LINEAR]     = {0},
	[ODIN_PFIM_FBCDC_8X8_V12]  = {.tiles_per_line = 8,
				      .tile_type = ODN_PFIM_TILETYPE_8X8,
				      .tile_xsize = 8,
				      .tile_ysize = 8},
	[ODIN_PFIM_FBCDC_16X4_V12] = {.tiles_per_line = 16,
				      .tile_type = ODN_PFIM_TILETYPE_16X4,
				      .tile_xsize = 16,
				      .tile_ysize = 4},
};

static const u32 GRPH_SURF_OFFSET[] = {
	ODN_PDP_GRPH1SURF_OFFSET,
	ODN_PDP_GRPH2SURF_OFFSET,
	ODN_PDP_VID1SURF_OFFSET,
	ODN_PDP_GRPH4SURF_OFFSET
};
static const u32 GRPH_SURF_GRPH_PIXFMT_SHIFT[] = {
	ODN_PDP_GRPH1SURF_GRPH1PIXFMT_SHIFT,
	ODN_PDP_GRPH2SURF_GRPH2PIXFMT_SHIFT,
	ODN_PDP_VID1SURF_VID1PIXFMT_SHIFT,
	ODN_PDP_GRPH4SURF_GRPH4PIXFMT_SHIFT
};
static const u32 GRPH_SURF_GRPH_PIXFMT_MASK[] = {
	ODN_PDP_GRPH1SURF_GRPH1PIXFMT_MASK,
	ODN_PDP_GRPH2SURF_GRPH2PIXFMT_MASK,
	ODN_PDP_VID1SURF_VID1PIXFMT_MASK,
	ODN_PDP_GRPH4SURF_GRPH4PIXFMT_MASK
};
static const u32 GRPH_GALPHA_OFFSET[] = {
	ODN_PDP_GRPH1GALPHA_OFFSET,
	ODN_PDP_GRPH2GALPHA_OFFSET,
	ODN_PDP_VID1GALPHA_OFFSET,
	ODN_PDP_GRPH4GALPHA_OFFSET
};
static const u32 GRPH_GALPHA_GRPH_GALPHA_SHIFT[] = {
	ODN_PDP_GRPH1GALPHA_GRPH1GALPHA_SHIFT,
	ODN_PDP_GRPH2GALPHA_GRPH2GALPHA_SHIFT,
	ODN_PDP_VID1GALPHA_VID1GALPHA_SHIFT,
	ODN_PDP_GRPH4GALPHA_GRPH4GALPHA_SHIFT
};
static const u32 GRPH_GALPHA_GRPH_GALPHA_MASK[] = {
	ODN_PDP_GRPH1GALPHA_GRPH1GALPHA_MASK,
	ODN_PDP_GRPH2GALPHA_GRPH2GALPHA_MASK,
	ODN_PDP_VID1GALPHA_VID1GALPHA_MASK,
	ODN_PDP_GRPH4GALPHA_GRPH4GALPHA_MASK
};
static const u32 GRPH_CTRL_OFFSET[] = {
	ODN_PDP_GRPH1CTRL_OFFSET,
	ODN_PDP_GRPH2CTRL_OFFSET,
	ODN_PDP_VID1CTRL_OFFSET,
	ODN_PDP_GRPH4CTRL_OFFSET,
};
static const u32 GRPH_CTRL_GRPH_BLEND_SHIFT[] = {
	ODN_PDP_GRPH1CTRL_GRPH1BLEND_SHIFT,
	ODN_PDP_GRPH2CTRL_GRPH2BLEND_SHIFT,
	ODN_PDP_VID1CTRL_VID1BLEND_SHIFT,
	ODN_PDP_GRPH4CTRL_GRPH4BLEND_SHIFT
};
static const u32 GRPH_CTRL_GRPH_BLEND_MASK[] = {
	ODN_PDP_GRPH1CTRL_GRPH1BLEND_MASK,
	ODN_PDP_GRPH2CTRL_GRPH2BLEND_MASK,
	ODN_PDP_VID1CTRL_VID1BLEND_MASK,
	ODN_PDP_GRPH4CTRL_GRPH4BLEND_MASK
};
static const u32 GRPH_CTRL_GRPH_BLENDPOS_SHIFT[] = {
	ODN_PDP_GRPH1CTRL_GRPH1BLENDPOS_SHIFT,
	ODN_PDP_GRPH2CTRL_GRPH2BLENDPOS_SHIFT,
	ODN_PDP_VID1CTRL_VID1BLENDPOS_SHIFT,
	ODN_PDP_GRPH4CTRL_GRPH4BLENDPOS_SHIFT
};
static const u32 GRPH_CTRL_GRPH_BLENDPOS_MASK[] = {
	ODN_PDP_GRPH1CTRL_GRPH1BLENDPOS_MASK,
	ODN_PDP_GRPH2CTRL_GRPH2BLENDPOS_MASK,
	ODN_PDP_VID1CTRL_VID1BLENDPOS_MASK,
	ODN_PDP_GRPH4CTRL_GRPH4BLENDPOS_MASK
};
static const u32 GRPH_CTRL_GRPH_STREN_SHIFT[] = {
	ODN_PDP_GRPH1CTRL_GRPH1STREN_SHIFT,
	ODN_PDP_GRPH2CTRL_GRPH2STREN_SHIFT,
	ODN_PDP_VID1CTRL_VID1STREN_SHIFT,
	ODN_PDP_GRPH4CTRL_GRPH4STREN_SHIFT
};
static const u32 GRPH_CTRL_GRPH_STREN_MASK[] = {
	ODN_PDP_GRPH1CTRL_GRPH1STREN_MASK,
	ODN_PDP_GRPH2CTRL_GRPH2STREN_MASK,
	ODN_PDP_VID1CTRL_VID1STREN_MASK,
	ODN_PDP_GRPH4CTRL_GRPH4STREN_MASK
};
static const u32 GRPH_POSN_OFFSET[] = {
	ODN_PDP_GRPH1POSN_OFFSET,
	ODN_PDP_GRPH2POSN_OFFSET,
	ODN_PDP_VID1POSN_OFFSET,
	ODN_PDP_GRPH4POSN_OFFSET
};
static const u32 GRPH_POSN_GRPH_XSTART_SHIFT[] = {
	ODN_PDP_GRPH1POSN_GRPH1XSTART_SHIFT,
	ODN_PDP_GRPH2POSN_GRPH2XSTART_SHIFT,
	ODN_PDP_VID1POSN_VID1XSTART_SHIFT,
	ODN_PDP_GRPH4POSN_GRPH4XSTART_SHIFT,
};
static const u32 GRPH_POSN_GRPH_XSTART_MASK[] = {
	ODN_PDP_GRPH1POSN_GRPH1XSTART_MASK,
	ODN_PDP_GRPH2POSN_GRPH2XSTART_MASK,
	ODN_PDP_VID1POSN_VID1XSTART_MASK,
	ODN_PDP_GRPH4POSN_GRPH4XSTART_MASK,
};
static const u32 GRPH_POSN_GRPH_YSTART_SHIFT[] = {
	ODN_PDP_GRPH1POSN_GRPH1YSTART_SHIFT,
	ODN_PDP_GRPH2POSN_GRPH2YSTART_SHIFT,
	ODN_PDP_VID1POSN_VID1YSTART_SHIFT,
	ODN_PDP_GRPH4POSN_GRPH4YSTART_SHIFT,
};
static const u32 GRPH_POSN_GRPH_YSTART_MASK[] = {
	ODN_PDP_GRPH1POSN_GRPH1YSTART_MASK,
	ODN_PDP_GRPH2POSN_GRPH2YSTART_MASK,
	ODN_PDP_VID1POSN_VID1YSTART_MASK,
	ODN_PDP_GRPH4POSN_GRPH4YSTART_MASK,
};
static const u32 GRPH_SIZE_OFFSET[] = {
	ODN_PDP_GRPH1SIZE_OFFSET,
	ODN_PDP_GRPH2SIZE_OFFSET,
	ODN_PDP_VID1SIZE_OFFSET,
	ODN_PDP_GRPH4SIZE_OFFSET,
};
static const u32 GRPH_SIZE_GRPH_WIDTH_SHIFT[] = {
	ODN_PDP_GRPH1SIZE_GRPH1WIDTH_SHIFT,
	ODN_PDP_GRPH2SIZE_GRPH2WIDTH_SHIFT,
	ODN_PDP_VID1SIZE_VID1WIDTH_SHIFT,
	ODN_PDP_GRPH4SIZE_GRPH4WIDTH_SHIFT
};
static const u32 GRPH_SIZE_GRPH_WIDTH_MASK[] = {
	ODN_PDP_GRPH1SIZE_GRPH1WIDTH_MASK,
	ODN_PDP_GRPH2SIZE_GRPH2WIDTH_MASK,
	ODN_PDP_VID1SIZE_VID1WIDTH_MASK,
	ODN_PDP_GRPH4SIZE_GRPH4WIDTH_MASK
};
static const u32 GRPH_SIZE_GRPH_HEIGHT_SHIFT[] = {
	ODN_PDP_GRPH1SIZE_GRPH1HEIGHT_SHIFT,
	ODN_PDP_GRPH2SIZE_GRPH2HEIGHT_SHIFT,
	ODN_PDP_VID1SIZE_VID1HEIGHT_SHIFT,
	ODN_PDP_GRPH4SIZE_GRPH4HEIGHT_SHIFT
};
static const u32 GRPH_SIZE_GRPH_HEIGHT_MASK[] = {
	ODN_PDP_GRPH1SIZE_GRPH1HEIGHT_MASK,
	ODN_PDP_GRPH2SIZE_GRPH2HEIGHT_MASK,
	ODN_PDP_VID1SIZE_VID1HEIGHT_MASK,
	ODN_PDP_GRPH4SIZE_GRPH4HEIGHT_MASK
};
static const u32 GRPH_STRIDE_OFFSET[] = {
	ODN_PDP_GRPH1STRIDE_OFFSET,
	ODN_PDP_GRPH2STRIDE_OFFSET,
	ODN_PDP_VID1STRIDE_OFFSET,
	ODN_PDP_GRPH4STRIDE_OFFSET
};
static const u32 GRPH_STRIDE_GRPH_STRIDE_SHIFT[] = {
	ODN_PDP_GRPH1STRIDE_GRPH1STRIDE_SHIFT,
	ODN_PDP_GRPH2STRIDE_GRPH2STRIDE_SHIFT,
	ODN_PDP_VID1STRIDE_VID1STRIDE_SHIFT,
	ODN_PDP_GRPH4STRIDE_GRPH4STRIDE_SHIFT
};
static const u32 GRPH_STRIDE_GRPH_STRIDE_MASK[] = {
	ODN_PDP_GRPH1STRIDE_GRPH1STRIDE_MASK,
	ODN_PDP_GRPH2STRIDE_GRPH2STRIDE_MASK,
	ODN_PDP_VID1STRIDE_VID1STRIDE_MASK,
	ODN_PDP_GRPH4STRIDE_GRPH4STRIDE_MASK
};
static const u32 GRPH_INTERLEAVE_CTRL_OFFSET[] = {
	ODN_PDP_GRPH1INTERLEAVE_CTRL_OFFSET,
	ODN_PDP_GRPH2INTERLEAVE_CTRL_OFFSET,
	ODN_PDP_VID1INTERLEAVE_CTRL_OFFSET,
	ODN_PDP_GRPH4INTERLEAVE_CTRL_OFFSET
};
static const u32 GRPH_INTERLEAVE_CTRL_GRPH_INTFIELD_SHIFT[] = {
	ODN_PDP_GRPH1INTERLEAVE_CTRL_GRPH1INTFIELD_SHIFT,
	ODN_PDP_GRPH2INTERLEAVE_CTRL_GRPH2INTFIELD_SHIFT,
	ODN_PDP_VID1INTERLEAVE_CTRL_VID1INTFIELD_SHIFT,
	ODN_PDP_GRPH4INTERLEAVE_CTRL_GRPH4INTFIELD_SHIFT
};
static const u32 GRPH_INTERLEAVE_CTRL_GRPH_INTFIELD_MASK[] = {
	ODN_PDP_GRPH1INTERLEAVE_CTRL_GRPH1INTFIELD_MASK,
	ODN_PDP_GRPH2INTERLEAVE_CTRL_GRPH2INTFIELD_MASK,
	ODN_PDP_VID1INTERLEAVE_CTRL_VID1INTFIELD_MASK,
	ODN_PDP_GRPH4INTERLEAVE_CTRL_GRPH4INTFIELD_MASK
};
static const u32 GRPH_BASEADDR_OFFSET[] = {
	ODN_PDP_GRPH1BASEADDR_OFFSET,
	ODN_PDP_GRPH2BASEADDR_OFFSET,
	ODN_PDP_VID1BASEADDR_OFFSET,
	ODN_PDP_GRPH4BASEADDR_OFFSET
};

static const u32 ODN_INTERNAL_RESETN_PDP_MASK[] = {
	ODN_INTERNAL_RESETN_PDP1_MASK,
	ODN_INTERNAL_RESETN_PDP2_MASK
};

static const u32 ODN_INTERNAL_RESETN_PDP_SHIFT[]  = {
	ODN_INTERNAL_RESETN_PDP1_SHIFT,
	ODN_INTERNAL_RESETN_PDP2_SHIFT
};

static void get_odin_clock_settings(u32 value, u32 *lo_time, u32 *hi_time,
				u32 *no_count, u32 *edge)
{
	u32 lt, ht;

	/* If the value is 1, High Time & Low Time are both set to 1
	 * and the NOCOUNT bit is set to 1.
	 */
	if (value == 1) {
		*lo_time = 1;
		*hi_time = 1;

		/* If od is an odd number then write 1 to NO_COUNT
		 * otherwise write 0.
		 */
		*no_count = 1;

		/* If m is and odd number then write 1 to EDGE bit of MR2
		 * otherwise write 0.
		 * If id is an odd number then write 1 to EDGE bit of ID
		 * otherwise write 0.
		 */
		*edge = 0;
		return;
	}
	*no_count = 0;

	/* High Time & Low time is half the value listed for each PDP mode */
	lt = value>>1;
	ht = lt;

	/* If the value is odd, Low Time is rounded up to nearest integer
	 * and High Time is rounded down, and Edge is set to 1.
	 */
	if (value & 1) {
		lt++;

		/* If m is and odd number then write 1 to EDGE bit of MR2
		 * otherwise write 0.
		 * If id is an odd number then write 1 to EDGE bit of ID
		 * otherwise write 0.
		 */
		*edge = 1;

	} else {
		*edge = 0;
	}
	*hi_time = ht;
	*lo_time = lt;
}

static const struct odin_displaymode *get_odin_mode(int w, int h,
						    enum pdp_odin_subversion pv)
{
	struct odin_displaymode *pdp_modes;
	int n = 0;

	if (pv == PDP_ODIN_ORION)
		pdp_modes = (struct odin_displaymode *)orion_modes;
	else
		pdp_modes = (struct odin_displaymode *)odin_modes;

	do {
		if ((pdp_modes[n].w == w) && (pdp_modes[n].h == h))
			return pdp_modes+n;

	} while (pdp_modes[n++].w);

	return NULL;
}

bool pdp_odin_clocks_set(struct device *dev,
			 void __iomem *pdp_reg, void __iomem *pll_reg,
			 u32 clock_freq, u32 dev_num,
			 void __iomem *odn_core_reg,
			 u32 hdisplay, u32 vdisplay,
			 enum pdp_odin_subversion pdpsubv)
{
	u32 value;
	const struct odin_displaymode *odispl;
	u32 hi_time, lo_time, no_count, edge;
	u32 core_id, core_rev;

	core_id = pdp_rreg32(pdp_reg, ODN_PDP_CORE_ID_OFFSET);
	dev_info(dev, "Odin-PDP CORE_ID  %08X\n", core_id);

	core_rev = pdp_rreg32(odn_core_reg, ODN_PDP_CORE_REV_OFFSET);
	dev_info(dev, "Odin-PDP CORE_REV %08X\n", core_rev);

	odispl = get_odin_mode(hdisplay, vdisplay, pdpsubv);
	if (!odispl) {
		dev_err(dev, "Display mode not supported.\n");
		return false;
	}

	/*
	 * The PDP uses a Xilinx clock that requires read
	 * modify write for all registers.
	 * It is essential that only the specified bits are changed
	 * because other bits are in use.
	 * To change PDP clocks reset PDP & PDP mmcm (PLL) first,
	 * then apply changes and then un-reset mmcm & PDP.
	 * Warm reset will keep the changes.
	 *    wr 0x000080 0x1f7 ; # reset pdp
	 *    wr 0x000090 8 ; # reset pdp mmcm
	 * then apply clock changes, then
	 *    wr 0x000090 0x0 ; # un-reset pdp mmcm
	 *    wr 0x000080 0x1ff ; # un-reset pdp
	 */

	/*
	 * Hold Odin PDP in reset while changing the clock regs.
	 * Set the PDP bit of ODN_CORE_INTERNAL_RESETN low to reset.
	 * set bit 3 to 0 (active low)
	 */
	if (pdpsubv == PDP_ODIN_ORION) {
		value = core_rreg32(odn_core_reg, SRS_CORE_SOFT_RESETN);
		value = REG_VALUE_LO(value, 1, SRS_SOFT_RESETN_PDP_SHIFT,
				     SRS_SOFT_RESETN_PDP_MASK);
		core_wreg32(odn_core_reg, SRS_CORE_SOFT_RESETN, value);
	} else {
		value = core_rreg32(odn_core_reg, ODN_CORE_INTERNAL_RESETN);
		value = REG_VALUE_LO(value, 1,
				     ODN_INTERNAL_RESETN_PDP_SHIFT[dev_num],
				     ODN_INTERNAL_RESETN_PDP_MASK[dev_num]);
		core_wreg32(odn_core_reg, ODN_CORE_INTERNAL_RESETN, value);
	}

	/*
	 * Hold the PDP MMCM in reset while changing the clock regs.
	 * Set the PDP bit of ODN_CORE_CLK_GEN_RESET high to reset.
	 */
	value = core_rreg32(odn_core_reg, ODN_CORE_CLK_GEN_RESET);
	value = REG_VALUE_SET(value, 0x1,
			      ODN_CLK_GEN_RESET_PDP_MMCM_SHIFT,
			      ODN_CLK_GEN_RESET_PDP_MMCM_MASK);
	core_wreg32(odn_core_reg, ODN_CORE_CLK_GEN_RESET, value);

	/* Pixel clock Input divider */
	get_odin_clock_settings(odispl->id, &lo_time, &hi_time,
				&no_count, &edge);

	value = pll_rreg32(pll_reg,
			   ODIN_PLL_REG(ODN_PDP_P_CLK_IN_DIVIDER_REG));
	value = REG_VALUE_SET(value, lo_time,
			      ODN_PDP_PCLK_IDIV_LO_TIME_SHIFT,
			      ODN_PDP_PCLK_IDIV_LO_TIME_MASK);
	value = REG_VALUE_SET(value, hi_time,
			      ODN_PDP_PCLK_IDIV_HI_TIME_SHIFT,
			      ODN_PDP_PCLK_IDIV_HI_TIME_MASK);
	value = REG_VALUE_SET(value, no_count,
			      ODN_PDP_PCLK_IDIV_NOCOUNT_SHIFT,
			      ODN_PDP_PCLK_IDIV_NOCOUNT_MASK);
	value = REG_VALUE_SET(value, edge,
			      ODN_PDP_PCLK_IDIV_EDGE_SHIFT,
			      ODN_PDP_PCLK_IDIV_EDGE_MASK);
	pll_wreg32(pll_reg, ODIN_PLL_REG(ODN_PDP_P_CLK_IN_DIVIDER_REG),
		   value);

	/* Pixel clock Output divider */
	get_odin_clock_settings(odispl->od1, &lo_time, &hi_time,
				&no_count, &edge);

	/* Pixel clock Output divider reg1 */
	value = pll_rreg32(pll_reg,
			   ODIN_PLL_REG(ODN_PDP_P_CLK_OUT_DIVIDER_REG1));
	value = REG_VALUE_SET(value, lo_time,
			      ODN_PDP_PCLK_ODIV1_LO_TIME_SHIFT,
			      ODN_PDP_PCLK_ODIV1_LO_TIME_MASK);
	value = REG_VALUE_SET(value, hi_time,
			      ODN_PDP_PCLK_ODIV1_HI_TIME_SHIFT,
			      ODN_PDP_PCLK_ODIV1_HI_TIME_MASK);
	pll_wreg32(pll_reg, ODIN_PLL_REG(ODN_PDP_P_CLK_OUT_DIVIDER_REG1),
		   value);

	/* Pixel clock Output divider reg2 */
	value = pll_rreg32(pll_reg,
			   ODIN_PLL_REG(ODN_PDP_P_CLK_OUT_DIVIDER_REG2));
	value = REG_VALUE_SET(value, no_count,
			      ODN_PDP_PCLK_ODIV2_NOCOUNT_SHIFT,
			      ODN_PDP_PCLK_ODIV2_NOCOUNT_MASK);
	value = REG_VALUE_SET(value, edge,
			      ODN_PDP_PCLK_ODIV2_EDGE_SHIFT,
			      ODN_PDP_PCLK_ODIV2_EDGE_MASK);
	if (pdpsubv == PDP_ODIN_ORION) {
		/*
		 * Fractional divide for PLL registers currently does not work
		 * on Sirius, as duly mentioned on the TRM. However, owing to
		 * what most likely is a design flaw in the RTL, the
		 * following register and a later one have their fractional
		 * divide fields set to values other than 0 by default,
		 * unlike on Odin. This prevents the PDP device from working
		 * on Orion
		 */
		value = REG_VALUE_LO(value, 0x1F, SRS_PDP_PCLK_ODIV2_FRAC_SHIFT,
				     SRS_PDP_PCLK_ODIV2_FRAC_MASK);
	}
	pll_wreg32(pll_reg, ODIN_PLL_REG(ODN_PDP_P_CLK_OUT_DIVIDER_REG2),
		   value);

	/* Pixel clock Multiplier */
	get_odin_clock_settings(odispl->m, &lo_time, &hi_time,
				&no_count, &edge);

	/* Pixel clock Multiplier reg1 */
	value = pll_rreg32(pll_reg,
			   ODIN_PLL_REG(ODN_PDP_P_CLK_MULTIPLIER_REG1));
	value = REG_VALUE_SET(value, lo_time,
			      ODN_PDP_PCLK_MUL1_LO_TIME_SHIFT,
			      ODN_PDP_PCLK_MUL1_LO_TIME_MASK);
	value = REG_VALUE_SET(value, hi_time,
			      ODN_PDP_PCLK_MUL1_HI_TIME_SHIFT,
			      ODN_PDP_PCLK_MUL1_HI_TIME_MASK);
	pll_wreg32(pll_reg, ODIN_PLL_REG(ODN_PDP_P_CLK_MULTIPLIER_REG1),
		   value);

	/* Pixel clock Multiplier reg2 */
	value = pll_rreg32(pll_reg,
			   ODIN_PLL_REG(ODN_PDP_P_CLK_MULTIPLIER_REG2));
	value = REG_VALUE_SET(value, no_count,
			      ODN_PDP_PCLK_MUL2_NOCOUNT_SHIFT,
			      ODN_PDP_PCLK_MUL2_NOCOUNT_MASK);
	value = REG_VALUE_SET(value, edge,
			      ODN_PDP_PCLK_MUL2_EDGE_SHIFT,
			      ODN_PDP_PCLK_MUL2_EDGE_MASK);
	if (pdpsubv == PDP_ODIN_ORION) {
		/* Zero out fractional divide fields */
		value = REG_VALUE_LO(value, 0x1F, SRS_PDP_PCLK_MUL2_FRAC_SHIFT,
				     SRS_PDP_PCLK_MUL2_FRAC_MASK);
	}
	pll_wreg32(pll_reg, ODIN_PLL_REG(ODN_PDP_P_CLK_MULTIPLIER_REG2),
		   value);

	/* Mem clock Output divider */
	get_odin_clock_settings(odispl->od2, &lo_time, &hi_time,
				&no_count, &edge);

	/* Mem clock Output divider reg1 */
	value = pll_rreg32(pll_reg,
			   ODIN_PLL_REG(ODN_PDP_M_CLK_OUT_DIVIDER_REG1));
	value = REG_VALUE_SET(value, lo_time,
			      ODN_PDP_MCLK_ODIV1_LO_TIME_SHIFT,
			      ODN_PDP_MCLK_ODIV1_LO_TIME_MASK);
	value = REG_VALUE_SET(value, hi_time,
			      ODN_PDP_MCLK_ODIV1_HI_TIME_SHIFT,
			      ODN_PDP_MCLK_ODIV1_HI_TIME_MASK);
	pll_wreg32(pll_reg, ODIN_PLL_REG(ODN_PDP_M_CLK_OUT_DIVIDER_REG1),
		   value);

	/* Mem clock Output divider reg2 */
	value = pll_rreg32(pll_reg,
			   ODIN_PLL_REG(ODN_PDP_M_CLK_OUT_DIVIDER_REG2));
	value = REG_VALUE_SET(value, no_count,
			      ODN_PDP_MCLK_ODIV2_NOCOUNT_SHIFT,
			      ODN_PDP_MCLK_ODIV2_NOCOUNT_MASK);
	value = REG_VALUE_SET(value, edge,
			      ODN_PDP_MCLK_ODIV2_EDGE_SHIFT,
			      ODN_PDP_MCLK_ODIV2_EDGE_MASK);
	pll_wreg32(pll_reg, ODIN_PLL_REG(ODN_PDP_M_CLK_OUT_DIVIDER_REG2),
		   value);

	/*
	 * Take the PDP MMCM out of reset.
	 * Set the PDP bit of ODN_CORE_CLK_GEN_RESET to 0.
	 */
	value = core_rreg32(odn_core_reg, ODN_CORE_CLK_GEN_RESET);
	value = REG_VALUE_LO(value, 1, ODN_CLK_GEN_RESET_PDP_MMCM_SHIFT,
			     ODN_CLK_GEN_RESET_PDP_MMCM_MASK);
	core_wreg32(odn_core_reg, ODN_CORE_CLK_GEN_RESET, value);

	/*
	 * Wait until MMCM_LOCK_STATUS_PDPP bit is '1' in register
	 * MMCM_LOCK_STATUS. Issue an error if this does not
	 * go to '1' within 500ms.
	 */
	{
		int count;
		bool locked = false;

		for (count = 0; count < 10; count++) {
			value = core_rreg32(odn_core_reg,
					    ODN_CORE_MMCM_LOCK_STATUS);
			if (value & ODN_MMCM_LOCK_STATUS_PDPP) {
				locked = true;
				break;
			}
			msleep(50);
		}

		if (!locked) {
			dev_err(dev, "The MMCM pll did not lock\n");
			return false;
		}
	}

	/*
	 * Take Odin-PDP out of reset:
	 * Set the PDP bit of ODN_CORE_INTERNAL_RESETN to 1.
	 */
	if (pdpsubv == PDP_ODIN_ORION) {
		value = core_rreg32(odn_core_reg, SRS_CORE_SOFT_RESETN);
		value = REG_VALUE_SET(value, 1, SRS_SOFT_RESETN_PDP_SHIFT,
				     SRS_SOFT_RESETN_PDP_MASK);
		core_wreg32(odn_core_reg, SRS_CORE_SOFT_RESETN, value);
	} else {
		value = core_rreg32(odn_core_reg, ODN_CORE_INTERNAL_RESETN);
		value = REG_VALUE_SET(value, 1,
				      ODN_INTERNAL_RESETN_PDP_SHIFT[dev_num],
				      ODN_INTERNAL_RESETN_PDP_MASK[dev_num]);
		core_wreg32(odn_core_reg, ODN_CORE_INTERNAL_RESETN, value);
	}

	return true;
}

void pdp_odin_set_updates_enabled(struct device *dev, void __iomem *pdp_reg,
				  bool enable)
{
	u32 value = enable ?
		(1 << ODN_PDP_REGISTER_UPDATE_CTRL_USE_VBLANK_SHIFT |
		 1 << ODN_PDP_REGISTER_UPDATE_CTRL_REGISTERS_VALID_SHIFT) :
		0x0;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set updates: %s\n", enable ? "enable" : "disable");
#endif

	pdp_wreg32(pdp_reg, ODN_PDP_REGISTER_UPDATE_CTRL_OFFSET, value);
}

void pdp_odin_set_syncgen_enabled(struct device *dev, void __iomem *pdp_reg,
				  bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set syncgen: %s\n", enable ? "enable" : "disable");
#endif

	value = pdp_rreg32(pdp_reg, ODN_PDP_SYNCCTRL_OFFSET);

	value = REG_VALUE_SET(value,
			      enable ? ODN_SYNC_GEN_ENABLE : ODN_SYNC_GEN_DISABLE,
			      ODN_PDP_SYNCCTRL_SYNCACTIVE_SHIFT,
			      ODN_PDP_SYNCCTRL_SYNCACTIVE_MASK);

	/* Invert the pixel clock */
	value = REG_VALUE_SET(value, ODN_PIXEL_CLOCK_INVERTED,
			      ODN_PDP_SYNCCTRL_CLKPOL_SHIFT,
			      ODN_PDP_SYNCCTRL_CLKPOL_MASK);

	/* Set the Horizontal Sync Polarity to active high */
	value = REG_VALUE_LO(value, ODN_HSYNC_POLARITY_ACTIVE_HIGH,
			     ODN_PDP_SYNCCTRL_HSPOL_SHIFT,
			     ODN_PDP_SYNCCTRL_HSPOL_MASK);

	pdp_wreg32(pdp_reg, ODN_PDP_SYNCCTRL_OFFSET, value);

	/* Check for underruns when the sync generator
	 * is being turned off.
	 */
	if (!enable) {
		value = pdp_rreg32(pdp_reg, ODN_PDP_INTSTAT_OFFSET);
		value &= ODN_PDP_INTSTAT_ALL_OURUN_MASK;

		if (value) {
			dev_warn(dev, "underruns detected. status=0x%08X\n",
				 value);
		} else {
			dev_info(dev, "no underruns detected\n");
		}
	}
}

void pdp_odin_set_powerdwn_enabled(struct device *dev, void __iomem *pdp_reg,
				   bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set powerdwn: %s\n", enable ? "enable" : "disable");
#endif

	value = pdp_rreg32(pdp_reg, ODN_PDP_SYNCCTRL_OFFSET);

	value = REG_VALUE_SET(value, enable ? 0x1 : 0x0,
			      ODN_PDP_SYNCCTRL_POWERDN_SHIFT,
			      ODN_PDP_SYNCCTRL_POWERDN_MASK);

	pdp_wreg32(pdp_reg, ODN_PDP_SYNCCTRL_OFFSET, value);
}

void pdp_odin_set_vblank_enabled(struct device *dev, void __iomem *pdp_reg,
				 bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set vblank: %s\n", enable ? "enable" : "disable");
#endif

	pdp_wreg32(pdp_reg, ODN_PDP_INTCLR_OFFSET, ODN_PDP_INTCLR_ALL);

	value = pdp_rreg32(pdp_reg, ODN_PDP_INTENAB_OFFSET);
	value = REG_VALUE_SET(value, enable ? 0x1 : 0x0,
			      ODN_PDP_INTENAB_INTEN_VBLNK0_SHIFT,
			      ODN_PDP_INTENAB_INTEN_VBLNK0_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_INTENAB_OFFSET, value);
}

bool pdp_odin_check_and_clear_vblank(struct device *dev,
				     void __iomem *pdp_reg)
{
	u32 value;

	value = pdp_rreg32(pdp_reg, ODN_PDP_INTSTAT_OFFSET);

	if (REG_VALUE_GET(value,
			  ODN_PDP_INTSTAT_INTS_VBLNK0_SHIFT,
			  ODN_PDP_INTSTAT_INTS_VBLNK0_MASK)) {
		pdp_wreg32(pdp_reg, ODN_PDP_INTCLR_OFFSET,
			   (1 << ODN_PDP_INTCLR_INTCLR_VBLNK0_SHIFT));

		return true;
	}
	return false;
}

void pdp_odin_set_plane_enabled(struct device *dev, void __iomem *pdp_reg,
				u32 plane, bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set plane %u: %s\n",
		 plane, enable ? "enable" : "disable");
#endif

	if (plane > 3) {
		dev_err(dev, "Maximum of 4 planes are supported\n");
		return;
	}

	value = pdp_rreg32(pdp_reg, GRPH_CTRL_OFFSET[plane]);
	value = REG_VALUE_SET(value, enable ? 0x1 : 0x0,
			      GRPH_CTRL_GRPH_STREN_SHIFT[plane],
			      GRPH_CTRL_GRPH_STREN_MASK[plane]);
	pdp_wreg32(pdp_reg, GRPH_CTRL_OFFSET[plane], value);
}

void pdp_odin_reset_planes(struct device *dev, void __iomem *pdp_reg)
{
#ifdef PDP_VERBOSE
	dev_info(dev, "Reset planes\n");
#endif

	pdp_wreg32(pdp_reg, GRPH_CTRL_OFFSET[0], 0x00000000);
	pdp_wreg32(pdp_reg, GRPH_CTRL_OFFSET[1], 0x01000000);
	pdp_wreg32(pdp_reg, GRPH_CTRL_OFFSET[2], 0x02000000);
	pdp_wreg32(pdp_reg, GRPH_CTRL_OFFSET[3], 0x03000000);
}

static unsigned int pfim_pixel_format(u32 pdp_format)
{
	u32 pfim_pixformat;

	switch (pdp_format) {
	case ODN_PDP_SURF_PIXFMT_ARGB8888:
		pfim_pixformat = ODN_PFIM_PIXFMT_ARGB8888;
		break;
	case ODN_PDP_SURF_PIXFMT_RGB565:
		pfim_pixformat = ODN_PFIM_PIXFMT_RGB565;
		break;
	default:
		WARN(true, "Unknown Odin pixel format: %u defaulting to ARGB8888\n",
		     pdp_format);
		pfim_pixformat = ODN_PFIM_PIXFMT_ARGB8888;
	}

	return pfim_pixformat;
}

static unsigned int pfim_tiles_line(u32 width,
				    u32 pfim_format,
				    u32 fbc_mode)
{
	u32 bpp;
	u32 tpl;

	switch (pfim_format) {
	case ODN_PFIM_PIXFMT_ARGB8888:
		bpp = 32;
		break;
	case ODN_PFIM_PIXFMT_RGB565:
		bpp = 16;
		break;
	default:
		WARN(true, "Unknown PFIM pixel format: %u, defaulting to 32 bpp\n",
		     pfim_format);
		bpp = 32;
	}

	if (fbc_mode < ODIN_PFIM_FBCDC_MAX) {
		tpl = pfim_properties[fbc_mode].tiles_per_line;
	} else {
		WARN(true, "Unknown FBC compression format: %u, defaulting to 8X8_V12\n",
		     fbc_mode);
		tpl = pfim_properties[ODIN_PFIM_FBCDC_8X8_V12].tiles_per_line;
	}

	return ((width/tpl) / (32/bpp));
}

static void pfim_modeset(void __iomem *pfim_reg)
{
	u32 value;

	/*
	 * Odin PDP can address up to 32 bits of PCI BAR4,
	 * so this register is not necessary
	 */
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_YARGB_BASE_ADDR_MSB, 0x00);

	/*
	 * Following registers are only used with YUV buffers,
	 * which we currently do not support
	 */
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_UV_BASE_ADDR_LSB, 0x00);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_UV_BASE_ADDR_MSB, 0x00);
	pdp_wreg32(pfim_reg, CR_PFIM_PDP_Y_BASE_ADDR, 0x00);
	pdp_wreg32(pfim_reg, CR_PFIM_PDP_UV_BASE_ADDR, 0x00);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_CR_Y_VAL0, 0x00);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_CR_UV_VAL0, 0x00);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_CR_Y_VAL1, 0x00);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_CR_UV_VAL1, 0x00);

	/*
	 * PFIM tags are used for distinguishing between Y and UV plane
	 * request when that is the kind of format we use. Thus, any
	 * random value will do, as explained in the TRM
	 */
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_REQ_CONTEXT, 0x00);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_REQ_TAG, PFIM_RND_TAG);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_REQ_SB_TAG, 0x00);

	/* Default tile value if tile is found to be corrupted */
	value = REG_VALUE_SET(0, 0x01,
			      CR_PFIM_FBDC_FILTER_ENABLE_SHIFT,
			      CR_PFIM_FBDC_FILTER_ENABLE_MASK);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_FILTER_ENABLE, value);

	/* Recommended values for corrupt tile substitution */
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_CR_CH0123_VAL0, 0x00);
	value = REG_VALUE_SET(0, 0x01000000,
			      CR_PFIM_FBDC_CR_CH0123_VAL1_SHIFT,
			      CR_PFIM_FBDC_CR_CH0123_VAL1_MASK);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_CR_CH0123_VAL1, value);

	/* Only used when requesting a clear tile */
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_CLEAR_COLOUR_LSB, 0x00);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_CLEAR_COLOUR_MSB, 0x00);

	/* Current PDP revision does not support lossy formats */
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_REQ_LOSSY, 0x00);

	/* Force invalidation of FBC headers at beginning of render */
	value = REG_VALUE_SET(0, 0x01,
			      CR_PFIM_FBDC_HDR_INVAL_REQ_SHIFT,
			      CR_PFIM_FBDC_HDR_INVAL_REQ_MASK);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_HDR_INVAL_REQ, value);
}

static unsigned int pfim_num_tiles(struct device *dev, u32 width, u32 height,
				   u32 pfim_format, u32 fbc_mode)
{
	u32 phys_width, phys_height;
	u32 walign, halign;
	u32 tile_mult;
	u32 num_tiles;
	u32 bpp;

	switch (pfim_format) {
	case ODN_PFIM_PIXFMT_ARGB8888:
		bpp = 32;
		tile_mult = 4;
		break;
	case ODN_PFIM_PIXFMT_RGB565:
		bpp = 16;
		tile_mult = 2;
		break;
	default:
		dev_warn(dev, "WARNING: Wrong PFIM pixel format: %d\n",
			 pfim_format);
		return 0;
	}

	switch (fbc_mode) {
	case ODIN_PFIM_FBCDC_8X8_V12:
		switch (bpp) {
		case 16:	/* 16x8 */
			walign = 16;
			break;
		case 32:	/* 8x8 */
			walign = 8;
			break;
		default:
			dev_warn(dev, "WARNING: Wrong bit depth: %d\n",
				 bpp);
		}
		halign = 8;
		break;
	case ODIN_PFIM_FBCDC_16X4_V12:
		switch (bpp) {
		case 16:	/* 32x4 */
			walign = 32;
			break;
		case 32:	/* 16x4 */
			walign = 16;
			break;
		default:
			dev_warn(dev, "WARNING: Wrong bit depth: %d\n",
				 bpp);
			return 0;
		}
		halign = 4;
		break;
	default:
		dev_warn(dev, "WARNING: Wrong FBC compression format: %d\n",
			 fbc_mode);
		return 0;
	}

	phys_width = ALIGN(width, walign);
	phys_height = ALIGN(height, halign);
	num_tiles = phys_width / pfim_properties[fbc_mode].tile_xsize;
	num_tiles *= phys_height / pfim_properties[fbc_mode].tile_ysize;
	num_tiles *= tile_mult;
	num_tiles /= 4;

	return num_tiles ? num_tiles : 1;
}

static void pfim_set_surface(struct device *dev,
			     void __iomem *pfim_reg,
			     u32 width,
			     u32 height,
			     u32 pdp_format,
			     u32 fbc_mode)
{
	u32 pfim_pixformat = pfim_pixel_format(pdp_format);
	u32 tiles_line = pfim_tiles_line(width, pfim_pixformat, fbc_mode);
	u32 tile_type = pfim_properties[fbc_mode].tile_type;
	u32 num_tiles = pfim_num_tiles(dev, width, height,
				       pfim_pixformat, fbc_mode);

	pdp_wreg32(pfim_reg, CR_PFIM_NUM_TILES, num_tiles);
	pdp_wreg32(pfim_reg, CR_PFIM_TILES_PER_LINE, tiles_line);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_PIX_FORMAT, pfim_pixformat);
	pdp_wreg32(pfim_reg, CR_PFIM_FBDC_TILE_TYPE, tile_type);
}

void pdp_odin_set_surface(struct device *dev, void __iomem *pdp_reg,
			  u32 plane, u32 address, u32 offset,
			  u32 posx, u32 posy,
			  u32 width, u32 height, u32 stride,
			  u32 format, u32 alpha, bool blend,
			  void __iomem *pfim_reg, u32 fbcm)
{
	/*
	 * Use a blender based on the plane number (this defines the Z
	 * ordering)
	 */
	static const int GRPH_BLEND_POS[] = { 0x0, 0x1, 0x2, 0x3 };
	u32 blend_mode;
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev,
		 "Set surface: plane=%d pos=%d:%d size=%dx%d stride=%d format=%d alpha=%d address=0x%x\n",
		 plane, posx, posy, width, height, stride,
		 format, alpha, address);
#endif

	if (plane > 3) {
		dev_err(dev, "Maximum of 4 planes are supported\n");
		return;
	}

	if (address & 0xf)
		dev_warn(dev, "The frame buffer address is not aligned\n");

	if (fbcm && pfim_reg) {
		pfim_set_surface(dev, pfim_reg,
				 width, height,
				 format, fbcm);
		pdp_wreg32(pfim_reg, CR_PFIM_FBDC_YARGB_BASE_ADDR_LSB,
			   (address + offset) >> 6);
	} else
		pdp_wreg32(pdp_reg, GRPH_BASEADDR_OFFSET[plane], address);

	/* Pos */
	value = REG_VALUE_SET(0x0, posx,
			      GRPH_POSN_GRPH_XSTART_SHIFT[plane],
			      GRPH_POSN_GRPH_XSTART_MASK[plane]);
	value = REG_VALUE_SET(value, posy,
			      GRPH_POSN_GRPH_YSTART_SHIFT[plane],
			      GRPH_POSN_GRPH_YSTART_MASK[plane]);
	pdp_wreg32(pdp_reg, GRPH_POSN_OFFSET[plane], value);

	/* Size */
	value = REG_VALUE_SET(0x0, width - 1,
			      GRPH_SIZE_GRPH_WIDTH_SHIFT[plane],
			      GRPH_SIZE_GRPH_WIDTH_MASK[plane]);
	value = REG_VALUE_SET(value, height - 1,
			      GRPH_SIZE_GRPH_HEIGHT_SHIFT[plane],
			      GRPH_SIZE_GRPH_HEIGHT_MASK[plane]);
	pdp_wreg32(pdp_reg, GRPH_SIZE_OFFSET[plane], value);

	/* Stride */
	value = REG_VALUE_SET(0x0, (stride >> 4) - 1,
			      GRPH_STRIDE_GRPH_STRIDE_SHIFT[plane],
			      GRPH_STRIDE_GRPH_STRIDE_MASK[plane]);
	pdp_wreg32(pdp_reg, GRPH_STRIDE_OFFSET[plane], value);

	/* Interlace mode: progressive */
	value = REG_VALUE_SET(0x0, ODN_INTERLACE_DISABLE,
			      GRPH_INTERLEAVE_CTRL_GRPH_INTFIELD_SHIFT[plane],
			      GRPH_INTERLEAVE_CTRL_GRPH_INTFIELD_MASK[plane]);
	pdp_wreg32(pdp_reg, GRPH_INTERLEAVE_CTRL_OFFSET[plane], value);

	/* Format */
	value = REG_VALUE_SET(0x0, format,
			      GRPH_SURF_GRPH_PIXFMT_SHIFT[plane],
			      GRPH_SURF_GRPH_PIXFMT_MASK[plane]);
	pdp_wreg32(pdp_reg, GRPH_SURF_OFFSET[plane], value);

	/* Global alpha (0...1023) */
	value = REG_VALUE_SET(0x0, ((1024 * 256) / 255 * alpha) / 256,
			      GRPH_GALPHA_GRPH_GALPHA_SHIFT[plane],
			      GRPH_GALPHA_GRPH_GALPHA_MASK[plane]);
	pdp_wreg32(pdp_reg, GRPH_GALPHA_OFFSET[plane], value);
	value = pdp_rreg32(pdp_reg, GRPH_CTRL_OFFSET[plane]);

	/* Blend mode */
	if (blend) {
		if (alpha != 255)
			blend_mode = 0x2; /* 0b10 = global alpha blending */
		else
			blend_mode = 0x3; /* 0b11 = pixel alpha blending */
	} else {
		blend_mode = 0x0; /* 0b00 = no blending */
	}
	value = REG_VALUE_SET(value, blend_mode,
			      GRPH_CTRL_GRPH_BLEND_SHIFT[plane],
			      GRPH_CTRL_GRPH_BLEND_MASK[plane]);

	/* Blend position */
	value = REG_VALUE_SET(value, GRPH_BLEND_POS[plane],
			      GRPH_CTRL_GRPH_BLENDPOS_SHIFT[plane],
			      GRPH_CTRL_GRPH_BLENDPOS_MASK[plane]);
	pdp_wreg32(pdp_reg, GRPH_CTRL_OFFSET[plane], value);
}

void pdp_odin_mode_set(struct device *dev, void __iomem *pdp_reg,
		       u32 h_display, u32 v_display,
		       u32 hbps, u32 ht, u32 has,
		       u32 hlbs, u32 hfps, u32 hrbs,
		       u32 vbps, u32 vt, u32 vas,
		       u32 vtbs, u32 vfps, u32 vbbs,
		       bool nhsync, bool nvsync,
		       void __iomem *pfim_reg)
{
	u32 value;

	dev_info(dev, "Set mode: %dx%d\n", h_display, v_display);
#ifdef PDP_VERBOSE
	dev_info(dev, " ht: %d hbps %d has %d hlbs %d hfps %d hrbs %d\n",
		 ht, hbps, has, hlbs, hfps, hrbs);
	dev_info(dev, " vt: %d vbps %d vas %d vtbs %d vfps %d vbbs %d\n",
		 vt, vbps, vas, vtbs, vfps, vbbs);
#endif

	/* Border colour: 10bits per channel */
	pdp_wreg32(pdp_reg, ODN_PDP_BORDCOL_R_OFFSET, 0x0);
	pdp_wreg32(pdp_reg, ODN_PDP_BORDCOL_GB_OFFSET, 0x0);

	/* Background: 10bits per channel */
	value = pdp_rreg32(pdp_reg, ODN_PDP_BGNDCOL_AR_OFFSET);
	value = REG_VALUE_SET(value, 0x3ff,
			      ODN_PDP_BGNDCOL_AR_BGNDCOL_A_SHIFT,
			      ODN_PDP_BGNDCOL_AR_BGNDCOL_A_MASK);
	value = REG_VALUE_SET(value, 0x0,
			      ODN_PDP_BGNDCOL_AR_BGNDCOL_R_SHIFT,
			      ODN_PDP_BGNDCOL_AR_BGNDCOL_R_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_BGNDCOL_AR_OFFSET, value);

	value = pdp_rreg32(pdp_reg, ODN_PDP_BGNDCOL_GB_OFFSET);
	value = REG_VALUE_SET(value, 0x0,
			      ODN_PDP_BGNDCOL_GB_BGNDCOL_G_SHIFT,
			      ODN_PDP_BGNDCOL_GB_BGNDCOL_G_MASK);
	value = REG_VALUE_SET(value, 0x0,
			      ODN_PDP_BGNDCOL_GB_BGNDCOL_B_SHIFT,
			      ODN_PDP_BGNDCOL_GB_BGNDCOL_B_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_BGNDCOL_GB_OFFSET, value);
	pdp_wreg32(pdp_reg, ODN_PDP_BORDCOL_GB_OFFSET, 0x0);

	/* Update control */
	value = pdp_rreg32(pdp_reg, ODN_PDP_UPDCTRL_OFFSET);
	value = REG_VALUE_SET(value, 0x0,
			      ODN_PDP_UPDCTRL_UPDFIELD_SHIFT,
			      ODN_PDP_UPDCTRL_UPDFIELD_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_UPDCTRL_OFFSET, value);

	/* Horizontal timing */
	value = pdp_rreg32(pdp_reg, ODN_PDP_HSYNC1_OFFSET);
	value = REG_VALUE_SET(value, hbps,
			      ODN_PDP_HSYNC1_HBPS_SHIFT,
			      ODN_PDP_HSYNC1_HBPS_MASK);
	value = REG_VALUE_SET(value, ht,
			      ODN_PDP_HSYNC1_HT_SHIFT,
			      ODN_PDP_HSYNC1_HT_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_HSYNC1_OFFSET, value);

	value = pdp_rreg32(pdp_reg, ODN_PDP_HSYNC2_OFFSET);
	value = REG_VALUE_SET(value, has,
			      ODN_PDP_HSYNC2_HAS_SHIFT,
			      ODN_PDP_HSYNC2_HAS_MASK);
	value = REG_VALUE_SET(value, hlbs,
			      ODN_PDP_HSYNC2_HLBS_SHIFT,
			      ODN_PDP_HSYNC2_HLBS_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_HSYNC2_OFFSET, value);

	value = pdp_rreg32(pdp_reg, ODN_PDP_HSYNC3_OFFSET);
	value = REG_VALUE_SET(value, hfps,
			      ODN_PDP_HSYNC3_HFPS_SHIFT,
			      ODN_PDP_HSYNC3_HFPS_MASK);
	value = REG_VALUE_SET(value, hrbs,
			      ODN_PDP_HSYNC3_HRBS_SHIFT,
			      ODN_PDP_HSYNC3_HRBS_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_HSYNC3_OFFSET, value);

	/* Vertical timing */
	value = pdp_rreg32(pdp_reg, ODN_PDP_VSYNC1_OFFSET);
	value = REG_VALUE_SET(value, vbps,
			      ODN_PDP_VSYNC1_VBPS_SHIFT,
			      ODN_PDP_VSYNC1_VBPS_MASK);
	value = REG_VALUE_SET(value, vt,
			      ODN_PDP_VSYNC1_VT_SHIFT,
			      ODN_PDP_VSYNC1_VT_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_VSYNC1_OFFSET, value);

	value = pdp_rreg32(pdp_reg, ODN_PDP_VSYNC2_OFFSET);
	value = REG_VALUE_SET(value, vas,
			      ODN_PDP_VSYNC2_VAS_SHIFT,
			      ODN_PDP_VSYNC2_VAS_MASK);
	value = REG_VALUE_SET(value, vtbs,
			      ODN_PDP_VSYNC2_VTBS_SHIFT,
			      ODN_PDP_VSYNC2_VTBS_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_VSYNC2_OFFSET, value);

	value = pdp_rreg32(pdp_reg, ODN_PDP_VSYNC3_OFFSET);
	value = REG_VALUE_SET(value, vfps,
			      ODN_PDP_VSYNC3_VFPS_SHIFT,
			      ODN_PDP_VSYNC3_VFPS_MASK);
	value = REG_VALUE_SET(value, vbbs,
			      ODN_PDP_VSYNC3_VBBS_SHIFT,
			      ODN_PDP_VSYNC3_VBBS_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_VSYNC3_OFFSET, value);

	/* Horizontal data enable */
	value = pdp_rreg32(pdp_reg, ODN_PDP_HDECTRL_OFFSET);
	value = REG_VALUE_SET(value, hlbs,
			      ODN_PDP_HDECTRL_HDES_SHIFT,
			      ODN_PDP_HDECTRL_HDES_MASK);
	value = REG_VALUE_SET(value, hfps,
			      ODN_PDP_HDECTRL_HDEF_SHIFT,
			      ODN_PDP_HDECTRL_HDEF_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_HDECTRL_OFFSET, value);

	/* Vertical data enable */
	value = pdp_rreg32(pdp_reg, ODN_PDP_VDECTRL_OFFSET);
	value = REG_VALUE_SET(value, vtbs,
			      ODN_PDP_VDECTRL_VDES_SHIFT,
			      ODN_PDP_VDECTRL_VDES_MASK);
	value = REG_VALUE_SET(value, vfps,
			      ODN_PDP_VDECTRL_VDEF_SHIFT,
			      ODN_PDP_VDECTRL_VDEF_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_VDECTRL_OFFSET, value);

	/* Vertical event start and vertical fetch start */
	value = pdp_rreg32(pdp_reg, ODN_PDP_VEVENT_OFFSET);
	value = REG_VALUE_SET(value, vbps,
			      ODN_PDP_VEVENT_VFETCH_SHIFT,
			      ODN_PDP_VEVENT_VFETCH_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_VEVENT_OFFSET, value);

	/* Set up polarities of sync/blank */
	value = REG_VALUE_SET(0, 0x1,
			      ODN_PDP_SYNCCTRL_BLNKPOL_SHIFT,
			      ODN_PDP_SYNCCTRL_BLNKPOL_MASK);
	if (nhsync)
		value = REG_VALUE_SET(value, 0x1,
				      ODN_PDP_SYNCCTRL_HSPOL_SHIFT,
				      ODN_PDP_SYNCCTRL_HSPOL_MASK);
	if (nvsync)
		value = REG_VALUE_SET(value, 0x1,
				      ODN_PDP_SYNCCTRL_VSPOL_SHIFT,
				      ODN_PDP_SYNCCTRL_VSPOL_MASK);
	pdp_wreg32(pdp_reg, ODN_PDP_SYNCCTRL_OFFSET, value);

	/* PDP framebuffer compression setup */
	if (pfim_reg)
		pfim_modeset(pfim_reg);
}
