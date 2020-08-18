/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if !defined(__PDP_ODIN_H__)
#define __PDP_ODI_H__

#include "pdp_common.h"
#include "odin_pdp_regs.h"
#include "odin_regs.h"
#include "odin_defs.h"

#define ODIN_PLL_REG(n)	((n) - ODN_PDP_P_CLK_OUT_DIVIDER_REG1)

struct odin_displaymode {
	int w;		/* display width */
	int h;		/* display height */
	int id;		/* pixel clock input divider */
	int m;		/* pixel clock multiplier */
	int od1;	/* pixel clock output divider */
	int od2;	/* mem clock output divider */
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

/* Odin register R-W */
static inline u32 odin_pdp_rreg32(void __iomem *base, resource_size_t reg)
{
	return ioread32(base + reg);
}

static inline void odin_pdp_wreg32(void __iomem *base, resource_size_t reg,
								   u32 value)
{
	iowrite32(value, base + reg);
}

static inline u32 odin_pll_rreg32(void __iomem *base, resource_size_t reg)
{
	return ioread32(base + reg);
}

static inline void odin_pll_wreg32(void __iomem *base, resource_size_t reg,
								   u32 value)
{
	iowrite32(value, base + reg);
}

static inline u32 odin_core_rreg32(void __iomem *base, resource_size_t reg)
{
	return ioread32(base + reg);
}

static inline void odin_core_wreg32(void __iomem *base,
				resource_size_t reg,
				u32 value)
{
	iowrite32(value, base + reg);
}

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
		 *  otherwise write 0.
		 */
		*no_count = 1;

		/* If m is and odd number then write 1 to EDGE bit of MR2
		 * otherwise write 0.
		 * If id is an odd number then write 1 to EDGE bit of ID
		 *  otherwise write 0.
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

static const struct odin_displaymode *get_odin_mode(int w, int h)
{
	int n = 0;

	do {
		if ((odin_modes[n].w == w) && (odin_modes[n].h == h))
			return odin_modes+n;

	} while (odin_modes[n++].w);

	return NULL;
}

static bool pdp_odin_clocks_set(struct device *dev,
				void __iomem *pdp_reg, void __iomem *pll_reg,
				u32 clock_freq,
				void __iomem *odn_core_reg,
				u32 hdisplay, u32 vdisplay)
{
	u32 value;
	const struct odin_displaymode *odispl;
	u32 hi_time, lo_time, no_count, edge;
	u32 core_id, core_rev;

	core_id = odin_pdp_rreg32(pdp_reg,
				ODN_PDP_CORE_ID_OFFSET);
	dev_info(dev, "Odin-PDP CORE_ID  %08X\n", core_id);

	core_rev = odin_pdp_rreg32(odn_core_reg,
				ODN_PDP_CORE_REV_OFFSET);
	dev_info(dev, "Odin-PDP CORE_REV %08X\n", core_rev);

	odispl = get_odin_mode(hdisplay, vdisplay);
	if (!odispl) {
		dev_err(dev,
				"Error - display mode not supported.\n");
		return false;
	}

	/* The PDP uses a Xilinx clock that requires read
	 * modify write for all registers.
	 * It is essential that only the specified bits are changed
	 * because other bits are in use.
	 * To change PDP clocks reset PDP & PDP mmcm (PLL) first,
	 * then apply changes and then un-reset mmcm & PDP.
	 * Warm reset will keep the changes.
	 *    wr 0x000080 0x1f7 ; # reset pdp
	 *    wr 0x000090 8 ; # reset pdp  mmcm
	 * then apply clock changes, then
	 *    wr 0x000090 0x0 ; # un-reset pdp mmcm
	 *    wr 0x000080 0x1ff ; # un-reset pdp
	 */

	/* Hold Odin PDP1 in reset while changing the clock regs.
	 * Set the PDP1 bit of ODN_CORE_INTERNAL_RESETN low to reset.
	 * set bit 3 to 0 (active low)
	 */
	value = odin_core_rreg32(odn_core_reg,
				ODN_CORE_INTERNAL_RESETN);
	value = REG_VALUE_LO(value, 1, ODN_INTERNAL_RESETN_PDP1_SHIFT,
				ODN_INTERNAL_RESETN_PDP1_MASK);
	odin_core_wreg32(odn_core_reg,
				ODN_CORE_INTERNAL_RESETN, value);

	/* Hold the PDP MMCM in reset while changing the clock regs.
	 * Set the PDP1 bit of ODN_CORE_CLK_GEN_RESET high to reset.
	 */
	value = odin_core_rreg32(odn_core_reg,
				ODN_CORE_CLK_GEN_RESET);
	value = REG_VALUE_SET(value, 0x1,
				ODN_INTERNAL_RESETN_PDP1_SHIFT,
				ODN_INTERNAL_RESETN_PDP1_MASK);
	odin_core_wreg32(odn_core_reg,
				ODN_CORE_CLK_GEN_RESET, value);

	/* Pixel clock Input divider */
	get_odin_clock_settings(odispl->id, &lo_time, &hi_time,
				&no_count, &edge);

	value = odin_pll_rreg32(pll_reg,
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
	odin_pll_wreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_P_CLK_IN_DIVIDER_REG),
				value);

	/* Pixel clock Output divider */
	get_odin_clock_settings(odispl->od1, &lo_time, &hi_time,
				&no_count, &edge);

	/* Pixel clock Output divider reg1 */
	value = odin_pll_rreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_P_CLK_OUT_DIVIDER_REG1));
	value = REG_VALUE_SET(value, lo_time,
				ODN_PDP_PCLK_ODIV1_LO_TIME_SHIFT,
				ODN_PDP_PCLK_ODIV1_LO_TIME_MASK);
	value = REG_VALUE_SET(value, hi_time,
				ODN_PDP_PCLK_ODIV1_HI_TIME_SHIFT,
				ODN_PDP_PCLK_ODIV1_HI_TIME_MASK);
	odin_pll_wreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_P_CLK_OUT_DIVIDER_REG1),
				value);

	/* Pixel clock Output divider reg2 */
	value = odin_pll_rreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_P_CLK_OUT_DIVIDER_REG2));
	value = REG_VALUE_SET(value, no_count,
				ODN_PDP_PCLK_ODIV2_NOCOUNT_SHIFT,
				ODN_PDP_PCLK_ODIV2_NOCOUNT_MASK);
	value = REG_VALUE_SET(value, edge,
				ODN_PDP_PCLK_ODIV2_EDGE_SHIFT,
				ODN_PDP_PCLK_ODIV2_EDGE_MASK);
	odin_pll_wreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_P_CLK_OUT_DIVIDER_REG2),
				value);

	/* Pixel clock Multiplier */
	get_odin_clock_settings(odispl->m, &lo_time, &hi_time,
				&no_count, &edge);

	/* Pixel clock Multiplier reg1 */
	value = odin_pll_rreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_P_CLK_MULTIPLIER_REG1));
	value = REG_VALUE_SET(value, lo_time,
				ODN_PDP_PCLK_MUL1_LO_TIME_SHIFT,
				ODN_PDP_PCLK_MUL1_LO_TIME_MASK);
	value = REG_VALUE_SET(value, hi_time,
				ODN_PDP_PCLK_MUL1_HI_TIME_SHIFT,
				ODN_PDP_PCLK_MUL1_HI_TIME_MASK);
	odin_pll_wreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_P_CLK_MULTIPLIER_REG1),
				value);

	/* Pixel clock Multiplier reg2 */
	value = odin_pll_rreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_P_CLK_MULTIPLIER_REG2));
	value = REG_VALUE_SET(value, no_count,
				ODN_PDP_PCLK_MUL2_NOCOUNT_SHIFT,
				ODN_PDP_PCLK_MUL2_NOCOUNT_MASK);
	value = REG_VALUE_SET(value, edge,
				ODN_PDP_PCLK_MUL2_EDGE_SHIFT,
				ODN_PDP_PCLK_MUL2_EDGE_MASK);
	odin_pll_wreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_P_CLK_MULTIPLIER_REG2),
				value);

	/* Mem clock Output divider */
	get_odin_clock_settings(odispl->od2, &lo_time, &hi_time,
				&no_count, &edge);

	/* Mem clock Output divider reg1 */
	value = odin_pll_rreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_M_CLK_OUT_DIVIDER_REG1));
	value = REG_VALUE_SET(value, lo_time,
				ODN_PDP_MCLK_ODIV1_LO_TIME_SHIFT,
				ODN_PDP_MCLK_ODIV1_LO_TIME_MASK);
	value = REG_VALUE_SET(value, hi_time,
				ODN_PDP_MCLK_ODIV1_HI_TIME_SHIFT,
				ODN_PDP_MCLK_ODIV1_HI_TIME_MASK);
	odin_pll_wreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_M_CLK_OUT_DIVIDER_REG1),
				value);

	/* Mem clock Output divider reg2 */
	value = odin_pll_rreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_M_CLK_OUT_DIVIDER_REG2));
	value = REG_VALUE_SET(value, no_count,
				ODN_PDP_MCLK_ODIV2_NOCOUNT_SHIFT,
				ODN_PDP_MCLK_ODIV2_NOCOUNT_MASK);
	value = REG_VALUE_SET(value, edge,
				ODN_PDP_MCLK_ODIV2_EDGE_SHIFT,
				ODN_PDP_MCLK_ODIV2_EDGE_MASK);
	odin_pll_wreg32(pll_reg,
				ODIN_PLL_REG(ODN_PDP_M_CLK_OUT_DIVIDER_REG2),
				value);

	/* Take the PDP MMCM out of reset.
	 * Set the PDP1 bit of ODN_CORE_CLK_GEN_RESET to 0.
	 */
	value = odin_core_rreg32(odn_core_reg,
				ODN_CORE_CLK_GEN_RESET);
	value = REG_VALUE_LO(value, 1, ODN_INTERNAL_RESETN_PDP1_SHIFT,
				ODN_INTERNAL_RESETN_PDP1_MASK);
	odin_core_wreg32(odn_core_reg,
				ODN_CORE_CLK_GEN_RESET, value);

	/* Wait until MMCM_LOCK_STATUS_PDPP bit is ‘1’ in register
	 * MMCM_LOCK_STATUS. Issue an error if this does not
	 * go to ‘1’ within 500ms.
	 */
	{
		int count;
		bool locked = false;

		for (count = 0; count < 10; count++) {
			value = odin_core_rreg32(odn_core_reg,
						ODN_CORE_MMCM_LOCK_STATUS);
			if (value & ODN_MMCM_LOCK_STATUS_PDPP) {
				locked = true;
				break;
			}
			msleep(50);
		}

		if (!locked) {
			dev_err(dev,
					"Error - the MMCM pll did not lock\n");
			return false;
		}
	}

	/* Take Odin-PDP1 out of reset:
	 * Set the PDP1 bit of ODN_CORE_INTERNAL_RESETN to 1.
	 */
	value = odin_core_rreg32(odn_core_reg,
				ODN_CORE_INTERNAL_RESETN);
	value = REG_VALUE_SET(value, 1, ODN_INTERNAL_RESETN_PDP1_SHIFT,
				ODN_INTERNAL_RESETN_PDP1_MASK);
	odin_core_wreg32(odn_core_reg,
				ODN_CORE_INTERNAL_RESETN, value);

	return true;
}

static void pdp_odin_set_updates_enabled(struct device *dev,
				void __iomem *pdp_reg, bool enable)
{
	u32 value = enable ?
		(1 << ODN_PDP_REGISTER_UPDATE_CTRL_USE_VBLANK_SHIFT |
		 1 << ODN_PDP_REGISTER_UPDATE_CTRL_REGISTERS_VALID_SHIFT) :
		0x0;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set updates: %s\n",
			 enable ? "enable" : "disable");
#endif

	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_REGISTER_UPDATE_CTRL_OFFSET,
			value);
}

static void pdp_odin_set_syncgen_enabled(struct device *dev,
				void __iomem *pdp_reg, bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set syncgen: %s\n",
		enable ? "enable" : "disable");
#endif

	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_SYNCCTRL_OFFSET);

	value = REG_VALUE_SET(value,
		enable ? ODN_SYNC_GEN_ENABLE : ODN_SYNC_GEN_DISABLE,
		ODN_PDP_SYNCCTRL_SYNCACTIVE_SHIFT,
		ODN_PDP_SYNCCTRL_SYNCACTIVE_MASK);

	/* Invert the pixel clock */
	value = REG_VALUE_SET(value,  ODN_PIXEL_CLOCK_INVERTED,
		ODN_PDP_SYNCCTRL_CLKPOL_SHIFT,
		ODN_PDP_SYNCCTRL_CLKPOL_MASK);

	/* Set the Horizontal Sync Polarity to active high */
	value = REG_VALUE_LO(value, ODN_HSYNC_POLARITY_ACTIVE_HIGH,
			ODN_PDP_SYNCCTRL_HSPOL_SHIFT,
			ODN_PDP_SYNCCTRL_HSPOL_MASK);

	odin_pdp_wreg32(pdp_reg,
		ODN_PDP_SYNCCTRL_OFFSET,
		value);

	/* Check for underruns when the sync generator
	 * is being turned off.
	 */
	if (!enable) {
		value = odin_pdp_rreg32(pdp_reg,
				ODN_PDP_INTSTAT_OFFSET);
		value &= ODN_PDP_INTSTAT_ALL_OURUN_MASK;

		if (value)
			dev_warn(dev,
				"underruns detected. status=0x%08X\n",
				value);
		else
			dev_info(dev,
				"no underruns detected\n");
	}
}

static void pdp_odin_set_powerdwn_enabled(struct device *dev,
				void __iomem *pdp_reg, bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set powerdwn: %s\n",
		enable ? "enable" : "disable");
#endif

	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_SYNCCTRL_OFFSET);

	value = REG_VALUE_SET(value,
			enable ? 0x1 : 0x0,
			ODN_PDP_SYNCCTRL_POWERDN_SHIFT,
			ODN_PDP_SYNCCTRL_POWERDN_MASK);

	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_SYNCCTRL_OFFSET,
			value);
}

static void pdp_odin_set_vblank_enabled(struct device *dev,
				void __iomem *pdp_reg, bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set vblank: %s\n",
		enable ? "enable" : "disable");
#endif

	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_INTCLR_OFFSET,
			ODN_PDP_INTCLR_ALL);

	value = odin_pdp_rreg32(pdp_reg,
				ODN_PDP_INTENAB_OFFSET);
	value = REG_VALUE_SET(value,
			enable ? 0x1 : 0x0,
			ODN_PDP_INTENAB_INTEN_VBLNK0_SHIFT,
			ODN_PDP_INTENAB_INTEN_VBLNK0_MASK);
	value = enable ? (1 << ODN_PDP_INTENAB_INTEN_VBLNK0_SHIFT) : 0;
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_INTENAB_OFFSET, value);
}

static bool pdp_odin_check_and_clear_vblank(struct device *dev,
				void __iomem *pdp_reg)
{
	u32 value;

	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_INTSTAT_OFFSET);

	if (REG_VALUE_GET(value,
			ODN_PDP_INTSTAT_INTS_VBLNK0_SHIFT,
			ODN_PDP_INTSTAT_INTS_VBLNK0_MASK)) {
		odin_pdp_wreg32(pdp_reg,
			ODN_PDP_INTCLR_OFFSET,
			(1 << ODN_PDP_INTCLR_INTCLR_VBLNK0_SHIFT));

		return true;
	}
	return false;
}

static void pdp_odin_set_plane_enabled(struct device *dev,
				void __iomem *pdp_reg, u32 plane, bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set plane %u: %s\n",
		 plane, enable ? "enable" : "disable");
#endif

	if (plane > 3) {
		dev_err(dev,
			"Maximum of 4 planes are supported\n");
		return;
	}

	value = odin_pdp_rreg32(pdp_reg,
			GRPH_CTRL_OFFSET[plane]);
	value = REG_VALUE_SET(value,
			enable ? 0x1 : 0x0,
			GRPH_CTRL_GRPH_STREN_SHIFT[plane],
			GRPH_CTRL_GRPH_STREN_MASK[plane]);
	odin_pdp_wreg32(pdp_reg,
			GRPH_CTRL_OFFSET[plane], value);
}

static void pdp_odin_reset_planes(struct device *dev,
				void __iomem *pdp_reg)
{
#ifdef PDP_VERBOSE
	dev_info(dev, "Reset planes\n");
#endif

	odin_pdp_wreg32(pdp_reg,
			GRPH_CTRL_OFFSET[0], 0x00000000);
	odin_pdp_wreg32(pdp_reg,
			GRPH_CTRL_OFFSET[1], 0x01000000);
	odin_pdp_wreg32(pdp_reg,
			GRPH_CTRL_OFFSET[2], 0x02000000);
	odin_pdp_wreg32(pdp_reg,
			GRPH_CTRL_OFFSET[3], 0x03000000);
}

static void pdp_odin_set_surface(struct device *dev,
				void __iomem *pdp_reg,
				u32 plane,
				u32 address,
				u32 posx, u32 posy,
				u32 width, u32 height,
				u32 stride,
				u32 format,
				u32 alpha,
				bool blend)
{
	/* Use a blender based on the plane number (this defines the Z
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
		dev_err(dev,
			"Maximum of 4 planes are supported\n");
		return;
	}

	if (address & 0xf) {
		dev_warn(dev,
			 "The frame buffer address is not aligned\n");
	}

	/* Frame buffer base address */
	odin_pdp_wreg32(pdp_reg,
			GRPH_BASEADDR_OFFSET[plane],
			address);
	/* Pos */
	value = REG_VALUE_SET(0x0,
			posx,
			GRPH_POSN_GRPH_XSTART_SHIFT[plane],
			GRPH_POSN_GRPH_XSTART_MASK[plane]);
	value = REG_VALUE_SET(value,
			posy,
			GRPH_POSN_GRPH_YSTART_SHIFT[plane],
			GRPH_POSN_GRPH_YSTART_MASK[plane]);
	odin_pdp_wreg32(pdp_reg,
			GRPH_POSN_OFFSET[plane],
			value);
	/* Size */
	value = REG_VALUE_SET(0x0,
			width - 1,
			GRPH_SIZE_GRPH_WIDTH_SHIFT[plane],
			GRPH_SIZE_GRPH_WIDTH_MASK[plane]);
	value = REG_VALUE_SET(value,
			height - 1,
			GRPH_SIZE_GRPH_HEIGHT_SHIFT[plane],
			GRPH_SIZE_GRPH_HEIGHT_MASK[plane]);
	odin_pdp_wreg32(pdp_reg,
			GRPH_SIZE_OFFSET[plane],
			value);
	/* Stride */
	value = REG_VALUE_SET(0x0,
			(stride >> 4) - 1,
			GRPH_STRIDE_GRPH_STRIDE_SHIFT[plane],
			GRPH_STRIDE_GRPH_STRIDE_MASK[plane]);
	odin_pdp_wreg32(pdp_reg,
			GRPH_STRIDE_OFFSET[plane], value);
	/* Interlace mode: progressive */
	value = REG_VALUE_SET(0x0,
			ODN_INTERLACE_DISABLE,
			GRPH_INTERLEAVE_CTRL_GRPH_INTFIELD_SHIFT[plane],
			GRPH_INTERLEAVE_CTRL_GRPH_INTFIELD_MASK[plane]);
	odin_pdp_wreg32(pdp_reg,
			GRPH_INTERLEAVE_CTRL_OFFSET[plane],
			value);
	/* Format */
	value = REG_VALUE_SET(0x0,
			      format,
			      GRPH_SURF_GRPH_PIXFMT_SHIFT[plane],
			      GRPH_SURF_GRPH_PIXFMT_MASK[plane]);
	odin_pdp_wreg32(pdp_reg,
			GRPH_SURF_OFFSET[plane], value);
	/* Global alpha (0...1023) */
	value = REG_VALUE_SET(0x0,
			      ((1024*256)/255 * alpha)/256,
			      GRPH_GALPHA_GRPH_GALPHA_SHIFT[plane],
			      GRPH_GALPHA_GRPH_GALPHA_MASK[plane]);
	odin_pdp_wreg32(pdp_reg,
			GRPH_GALPHA_OFFSET[plane], value);
	value = odin_pdp_rreg32(pdp_reg,
				GRPH_CTRL_OFFSET[plane]);
	/* Blend mode */
	if (blend) {
		if (alpha != 255)
			blend_mode = 0x2; /* 0b10 = global alpha blending */
		else
			blend_mode = 0x3; /* 0b11 = pixel alpha blending */
	} else 
		blend_mode = 0x0; /* 0b00 = no blending */
	value = REG_VALUE_SET(0x0,
			      blend_mode,
			      GRPH_CTRL_GRPH_BLEND_SHIFT[plane],
			      GRPH_CTRL_GRPH_BLEND_MASK[plane]);
	/* Blend position */
	value = REG_VALUE_SET(value,
			      GRPH_BLEND_POS[plane],
			      GRPH_CTRL_GRPH_BLENDPOS_SHIFT[plane],
			      GRPH_CTRL_GRPH_BLENDPOS_MASK[plane]);
	odin_pdp_wreg32(pdp_reg,
			GRPH_CTRL_OFFSET[plane], value);
}

static void pdp_odin_mode_set(struct device *dev,
				void __iomem *pdp_reg,
				u32 h_display, u32 v_display,
				u32 hbps, u32 ht, u32 has,
				u32 hlbs, u32 hfps, u32 hrbs,
				u32 vbps, u32 vt, u32 vas,
				u32 vtbs, u32 vfps, u32 vbbs,
				bool nhsync, bool nvsync)
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
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_BORDCOL_R_OFFSET, 0x0);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_BORDCOL_GB_OFFSET, 0x0);
	/* Background: 10bits per channel */
	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_BGNDCOL_AR_OFFSET);
	value = REG_VALUE_SET(value, 0x3ff,
			ODN_PDP_BGNDCOL_AR_BGNDCOL_A_SHIFT,
			ODN_PDP_BGNDCOL_AR_BGNDCOL_A_MASK);
	value = REG_VALUE_SET(value, 0x0,
			ODN_PDP_BGNDCOL_AR_BGNDCOL_R_SHIFT,
			ODN_PDP_BGNDCOL_AR_BGNDCOL_R_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_BGNDCOL_AR_OFFSET, value);
	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_BGNDCOL_GB_OFFSET);
	value = REG_VALUE_SET(value, 0x0,
			ODN_PDP_BGNDCOL_GB_BGNDCOL_G_SHIFT,
			ODN_PDP_BGNDCOL_GB_BGNDCOL_G_MASK);
	value = REG_VALUE_SET(value, 0x0,
			ODN_PDP_BGNDCOL_GB_BGNDCOL_B_SHIFT,
			ODN_PDP_BGNDCOL_GB_BGNDCOL_B_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_BGNDCOL_GB_OFFSET, value);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_BORDCOL_GB_OFFSET, 0x0);
	/* Update control */
	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_UPDCTRL_OFFSET);
	value = REG_VALUE_SET(value, 0x0,
			ODN_PDP_UPDCTRL_UPDFIELD_SHIFT,
			ODN_PDP_UPDCTRL_UPDFIELD_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_UPDCTRL_OFFSET, value);

	/* Horizontal timing */
	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_HSYNC1_OFFSET);
	value = REG_VALUE_SET(value, hbps,
			ODN_PDP_HSYNC1_HBPS_SHIFT,
			ODN_PDP_HSYNC1_HBPS_MASK);
	value = REG_VALUE_SET(value, ht,
			ODN_PDP_HSYNC1_HT_SHIFT,
			ODN_PDP_HSYNC1_HT_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_HSYNC1_OFFSET, value);

	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_HSYNC2_OFFSET);
	value = REG_VALUE_SET(value, has,
			ODN_PDP_HSYNC2_HAS_SHIFT,
			ODN_PDP_HSYNC2_HAS_MASK);
	value = REG_VALUE_SET(value, hlbs,
			ODN_PDP_HSYNC2_HLBS_SHIFT,
			ODN_PDP_HSYNC2_HLBS_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_HSYNC2_OFFSET, value);

	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_HSYNC3_OFFSET);
	value = REG_VALUE_SET(value, hfps,
			ODN_PDP_HSYNC3_HFPS_SHIFT,
			ODN_PDP_HSYNC3_HFPS_MASK);
	value = REG_VALUE_SET(value, hrbs,
			ODN_PDP_HSYNC3_HRBS_SHIFT,
			ODN_PDP_HSYNC3_HRBS_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_HSYNC3_OFFSET, value);

	/* Vertical timing */
	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_VSYNC1_OFFSET);
	value = REG_VALUE_SET(value, vbps,
			ODN_PDP_VSYNC1_VBPS_SHIFT,
			ODN_PDP_VSYNC1_VBPS_MASK);
	value = REG_VALUE_SET(value, vt,
			ODN_PDP_VSYNC1_VT_SHIFT,
			ODN_PDP_VSYNC1_VT_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_VSYNC1_OFFSET, value);

	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_VSYNC2_OFFSET);
	value = REG_VALUE_SET(value, vas,
			ODN_PDP_VSYNC2_VAS_SHIFT,
			ODN_PDP_VSYNC2_VAS_MASK);
	value = REG_VALUE_SET(value, vtbs,
			ODN_PDP_VSYNC2_VTBS_SHIFT,
			ODN_PDP_VSYNC2_VTBS_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_VSYNC2_OFFSET, value);

	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_VSYNC3_OFFSET);
	value = REG_VALUE_SET(value, vfps,
			ODN_PDP_VSYNC3_VFPS_SHIFT,
			ODN_PDP_VSYNC3_VFPS_MASK);
	value = REG_VALUE_SET(value, vbbs,
			ODN_PDP_VSYNC3_VBBS_SHIFT,
			ODN_PDP_VSYNC3_VBBS_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_VSYNC3_OFFSET, value);

	/* Horizontal data enable */
	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_HDECTRL_OFFSET);
	value = REG_VALUE_SET(value, hlbs,
			ODN_PDP_HDECTRL_HDES_SHIFT,
			ODN_PDP_HDECTRL_HDES_MASK);
	value = REG_VALUE_SET(value, hfps,
			ODN_PDP_HDECTRL_HDEF_SHIFT,
			ODN_PDP_HDECTRL_HDEF_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_HDECTRL_OFFSET, value);

	/* Vertical data enable */
	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_VDECTRL_OFFSET);
	value = REG_VALUE_SET(value, vtbs,
			ODN_PDP_VDECTRL_VDES_SHIFT,
			ODN_PDP_VDECTRL_VDES_MASK);
	value = REG_VALUE_SET(value, vfps,
			ODN_PDP_VDECTRL_VDEF_SHIFT,
			ODN_PDP_VDECTRL_VDEF_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_VDECTRL_OFFSET, value);

	/* Vertical event start and vertical fetch start */
	value = odin_pdp_rreg32(pdp_reg,
			ODN_PDP_VEVENT_OFFSET);
	value = REG_VALUE_SET(value, vbps,
			ODN_PDP_VEVENT_VFETCH_SHIFT,
			ODN_PDP_VEVENT_VFETCH_MASK);
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_VEVENT_OFFSET, value);

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
	odin_pdp_wreg32(pdp_reg,
			ODN_PDP_SYNCCTRL_OFFSET,
			value);
}

#endif /* __PDP_ODIN_H__ */
