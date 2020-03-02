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

#if !defined(__PDP_APOLLO_H__)
#define __PDP_APOLLO_H__

#include "pdp_common.h"
#include "pdp_regs.h"
#include "tcf_rgbpdp_regs.h"
#include "tcf_pll.h"

/* Map a register to the "pll-regs" region */
#define PLL_REG(n) ((n) - TCF_PLL_PLL_PDP_CLK0)

/* Apollo register R-W */
static inline u32 pdp_rreg32(void __iomem *base, resource_size_t reg)
{
	return ioread32(base + reg);
}

static inline void pdp_wreg32(void __iomem *base, resource_size_t reg,
							  u32 value)
{
	iowrite32(value, base + reg);
}

static inline u32 pll_rreg32(void __iomem *base, resource_size_t reg)
{
	return ioread32(base + reg);
}

static inline void pll_wreg32(void __iomem *base, resource_size_t reg,
				u32 value)
{
	iowrite32(value, base + reg);
}

static bool pdp_apollo_clocks_set(struct device *dev,
				void __iomem *pdp_reg, void __iomem *pll_reg,
				u32 clock_in_mhz,
				void __iomem *odn_core_reg,
				u32 hdisplay, u32 vdisplay)
{
	u32 clock;

	/*
	 * Setup TCF_CR_PLL_PDP_CLK1TO5 based on the main clock speed
	 * (clock 0 or 3)
	 */
	clock = (clock_in_mhz >= 50) ? 0 : 0x3;

	/* Set phase 0, ratio 50:50 and frequency in MHz */
	pll_wreg32(pll_reg,
			   PLL_REG(TCF_PLL_PLL_PDP_CLK0), clock_in_mhz);

	pll_wreg32(pll_reg,
			   PLL_REG(TCF_PLL_PLL_PDP_CLK1TO5), clock);

	/* Now initiate reprogramming of the PLLs */
	pll_wreg32(pll_reg, PLL_REG(TCF_PLL_PLL_PDP_DRP_GO),
			   0x1);

	udelay(1000);

	pll_wreg32(pll_reg, PLL_REG(TCF_PLL_PLL_PDP_DRP_GO),
			   0x0);

	return true;
}

static void pdp_apollo_set_updates_enabled(struct device *dev,
				void __iomem *pdp_reg, bool enable)
{
#ifdef PDP_VERBOSE
	dev_info(dev, "Set updates: %s\n",
			 enable ? "enable" : "disable");
#endif

	/* nothing to do here */
}

static void pdp_apollo_set_syncgen_enabled(struct device *dev,
				void __iomem *pdp_reg, bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set syncgen: %s\n",
		enable ? "enable" : "disable");
#endif

	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_SYNCCTRL);
	value = REG_VALUE_SET(value,
						  enable ? 0x1 : 0x0,
						  SYNCACTIVE_SHIFT,
						  SYNCACTIVE_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_SYNCCTRL,
			   value);
}

static void pdp_apollo_set_powerdwn_enabled(struct device *dev,
				void __iomem *pdp_reg, bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set powerdwn: %s\n",
		enable ? "enable" : "disable");
#endif

	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_SYNCCTRL);
	value = REG_VALUE_SET(value,
						  enable ? 0x1 : 0x0,
						  POWERDN_SHIFT,
						  POWERDN_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_SYNCCTRL,
			   value);
}

static void pdp_apollo_set_vblank_enabled(struct device *dev,
				void __iomem *pdp_reg, bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set vblank: %s\n",
		enable ? "enable" : "disable");
#endif

	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_INTENAB);
	value = REG_VALUE_SET(value,
						  enable ? 0x1 : 0x0,
						  INTEN_VBLNK0_SHIFT,
						  INTEN_VBLNK0_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_INTENAB, value);
}

static bool pdp_apollo_check_and_clear_vblank(struct device *dev,
				void __iomem *pdp_reg)
{
	u32 value;

	value = pdp_rreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_INTSTAT);

	if (REG_VALUE_GET(value,
					INTS_VBLNK0_SHIFT,
					INTS_VBLNK0_MASK)) {
		value = REG_VALUE_SET(0,
					0x1,
					INTCLR_VBLNK0_SHIFT,
					INTCLR_VBLNK0_MASK);
		pdp_wreg32(pdp_reg,
				   TCF_RGBPDP_PVR_TCF_RGBPDP_INTCLEAR,
				   value);
		return true;
	}
	return false;
}

static void pdp_apollo_set_plane_enabled(struct device *dev,
				void __iomem *pdp_reg, u32 plane, bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set plane %u: %s\n",
		 plane, enable ? "enable" : "disable");
#endif

	if (plane > 0) {
		dev_err(dev,
			"Maximum of 1 plane is supported\n");
		return;
	}

	value = pdp_rreg32(pdp_reg,
					TCF_RGBPDP_PVR_TCF_RGBPDP_STR1ADDRCTRL);
	value = REG_VALUE_SET(value,
					enable ? 0x1 : 0x0,
					STR1STREN_SHIFT,
					STR1STREN_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_STR1ADDRCTRL,
			   value);
}

static void pdp_apollo_reset_planes(struct device *dev,
				void __iomem *pdp_reg)
{
#ifdef PDP_VERBOSE
	dev_info(dev, "Reset planes\n");
#endif

	pdp_apollo_set_plane_enabled(dev, pdp_reg, 0, false);
}

static void pdp_apollo_set_surface(struct device *dev,
				void __iomem *pdp_reg,
				u32 plane,
				u32 address,
				u32 posx, u32 posy,
				u32 width, u32 height, u32 stride,
				u32 format,
				u32 alpha,
				bool blend)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set surface: size=%dx%d stride=%d format=%d address=0x%x\n",
			 width, height, stride, format, address);
#endif

	if (plane > 0) {
		dev_err(dev,
			"Maximum of 1 plane is supported\n");
		return;
	}

	/* Size & format */
	value = pdp_rreg32(pdp_reg,
				TCF_RGBPDP_PVR_TCF_RGBPDP_STR1SURF);
	value = REG_VALUE_SET(value,
				width - 1,
				STR1WIDTH_SHIFT,
				STR1WIDTH_MASK);
	value = REG_VALUE_SET(value,
				height - 1,
				STR1HEIGHT_SHIFT,
				STR1HEIGHT_MASK);
	value = REG_VALUE_SET(value,
				format,
				STR1PIXFMT_SHIFT,
				STR1PIXFMT_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_STR1SURF,
			   value);
	/* Stride */
	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_PDP_STR1POSN);
	value = REG_VALUE_SET(value,
				(stride >> DCPDP_STR1POSN_STRIDE_SHIFT) - 1,
				STR1STRIDE_SHIFT,
				STR1STRIDE_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_PDP_STR1POSN, value);
	/* Disable interlaced output */
	value = pdp_rreg32(pdp_reg,
				TCF_RGBPDP_PVR_TCF_RGBPDP_STR1ADDRCTRL);
	value = REG_VALUE_SET(value,
				0x0,
				STR1INTFIELD_SHIFT,
				STR1INTFIELD_MASK);
	/* Frame buffer base address */
	value = REG_VALUE_SET(value,
				address >> DCPDP_STR1ADDRCTRL_BASE_ADDR_SHIFT,
				STR1BASE_SHIFT,
				STR1BASE_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_STR1ADDRCTRL,
			   value);
}

static void pdp_apollo_mode_set(struct device *dev,
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

#if 0
	/* I don't really know what this is doing but it was in the Android
	 * implementation (not in the Linux one). Seems not to be necessary
	 * though!
	 */
	if (pdp_rreg32(pdp_reg, TCF_RGBPDP_PVR_TCF_RGBPDP_STRCTRL)
		!= 0x0000C010) {
		/* Buffer request threshold */
		pdp_wreg32(pdp_reg, TCF_RGBPDP_PVR_TCF_RGBPDP_STRCTRL,
				   0x00001C10);
	}
#endif

	/* Border colour */
	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_BORDCOL);
	value = REG_VALUE_SET(value,
						  0x0,
						  BORDCOL_SHIFT,
						  BORDCOL_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_BORDCOL, value);

	/* Update control */
	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_UPDCTRL);
	value = REG_VALUE_SET(value,
						  0x0,
						  UPDFIELD_SHIFT,
						  UPDFIELD_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_UPDCTRL, value);

	/* Set hsync timings */
	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_HSYNC1);
	value = REG_VALUE_SET(value,
						  hbps,
						  HBPS_SHIFT,
						  HBPS_MASK);
	value = REG_VALUE_SET(value,
						  ht,
						  HT_SHIFT,
						  HT_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_HSYNC1, value);

	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_HSYNC2);
	value = REG_VALUE_SET(value,
						  has,
						  HAS_SHIFT,
						  HAS_MASK);
	value = REG_VALUE_SET(value,
						  hlbs,
						  HLBS_SHIFT,
						  HLBS_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_HSYNC2, value);

	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_HSYNC3);
	value = REG_VALUE_SET(value,
						  hfps,
						  HFPS_SHIFT,
						  HFPS_MASK);
	value = REG_VALUE_SET(value,
						  hrbs,
						  HRBS_SHIFT,
						  HRBS_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_HSYNC3, value);

	/* Set vsync timings */
	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_VSYNC1);
	value = REG_VALUE_SET(value,
						  vbps,
						  VBPS_SHIFT,
						  VBPS_MASK);
	value = REG_VALUE_SET(value,
						  vt,
						  VT_SHIFT,
						  VT_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_VSYNC1, value);

	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_VSYNC2);
	value = REG_VALUE_SET(value,
						  vas,
						  VAS_SHIFT,
						  VAS_MASK);
	value = REG_VALUE_SET(value,
						  vtbs,
						  VTBS_SHIFT,
						  VTBS_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_VSYNC2, value);

	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_VSYNC3);
	value = REG_VALUE_SET(value,
						  vfps,
						  VFPS_SHIFT,
						  VFPS_MASK);
	value = REG_VALUE_SET(value,
						  vbbs,
						  VBBS_SHIFT,
						  VBBS_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_VSYNC3, value);

	/* Horizontal data enable */
	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_HDECTRL);
	value = REG_VALUE_SET(value,
						  hlbs,
						  HDES_SHIFT,
						  HDES_MASK);
	value = REG_VALUE_SET(value,
						  hfps,
						  HDEF_SHIFT,
						  HDEF_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_HDECTRL, value);

	/* Vertical data enable */
	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_VDECTRL);
	value = REG_VALUE_SET(value,
						  vtbs,
						  VDES_SHIFT,
						  VDES_MASK);
	value = REG_VALUE_SET(value,
						  vfps,
						  VDEF_SHIFT,
						  VDEF_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_VDECTRL, value);

	/* Vertical event start and vertical fetch start */
	value = pdp_rreg32(pdp_reg,
					   TCF_RGBPDP_PVR_TCF_RGBPDP_VEVENT);
	value = REG_VALUE_SET(value,
						  vbps,
						  VFETCH_SHIFT,
						  VFETCH_MASK);
	value = REG_VALUE_SET(value,
						  vfps,
						  VEVENT_SHIFT,
						  VEVENT_MASK);
	pdp_wreg32(pdp_reg,
			   TCF_RGBPDP_PVR_TCF_RGBPDP_VEVENT, value);

	/* Set up polarities of sync/blank */
	value = REG_VALUE_SET(0,
						  0x1,
						  BLNKPOL_SHIFT,
						  BLNKPOL_MASK);
	/* Enable this if you want vblnk1. You also need to change to vblnk1
	 * in the interrupt handler.
	 */
#if 0
	value = REG_VALUE_SET(value,
						  0x1,
						  FIELDPOL_SHIFT,
						  FIELDPOL_MASK);
#endif
	if (nhsync)
		value = REG_VALUE_SET(value,
							  0x1,
							  HSPOL_SHIFT,
							  HSPOL_MASK);
	if (nvsync)
		value = REG_VALUE_SET(value,
							  0x1,
							  VSPOL_SHIFT,
							  VSPOL_MASK);
	pdp_wreg32(pdp_reg,
		TCF_RGBPDP_PVR_TCF_RGBPDP_SYNCCTRL,
		value);
}

#endif /* __PDP_APOLLO_H__ */
