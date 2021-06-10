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

#include "pdp_common.h"
#include "pdp_plato.h"
#include "pdp2_mmu_regs.h"
#include "pdp2_regs.h"

#define PLATO_PDP_STRIDE_SHIFT 5


void pdp_plato_set_syncgen_enabled(struct device *dev, void __iomem *pdp_reg,
				   bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set syncgen: %s\n", enable ? "enable" : "disable");
#endif

	value = pdp_rreg32(pdp_reg, PDP_SYNCCTRL_OFFSET);
	/* Starts Sync Generator. */
	value = REG_VALUE_SET(value, enable ? 0x1 : 0x0,
			      PDP_SYNCCTRL_SYNCACTIVE_SHIFT,
			      PDP_SYNCCTRL_SYNCACTIVE_MASK);
	/* Controls polarity of pixel clock: Pixel clock is inverted */
	value = REG_VALUE_SET(value, 0x01,
			      PDP_SYNCCTRL_CLKPOL_SHIFT,
			      PDP_SYNCCTRL_CLKPOL_MASK);
	pdp_wreg32(pdp_reg, PDP_SYNCCTRL_OFFSET, value);
}

void pdp_plato_set_vblank_enabled(struct device *dev, void __iomem *pdp_reg,
				  bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set vblank: %s\n", enable ? "enable" : "disable");
#endif

	pdp_wreg32(pdp_reg, PDP_INTCLR_OFFSET, 0xFFFFFFFF);

	value = pdp_rreg32(pdp_reg, PDP_INTENAB_OFFSET);
	value = REG_VALUE_SET(value, enable ? 0x1 : 0x0,
			      PDP_INTENAB_INTEN_VBLNK0_SHIFT,
			      PDP_INTENAB_INTEN_VBLNK0_MASK);
	pdp_wreg32(pdp_reg, PDP_INTENAB_OFFSET, value);
}

bool pdp_plato_check_and_clear_vblank(struct device *dev,
				      void __iomem *pdp_reg)
{
	u32 value;

	value = pdp_rreg32(pdp_reg, PDP_INTSTAT_OFFSET);

	if (REG_VALUE_GET(value,
			  PDP_INTSTAT_INTS_VBLNK0_SHIFT,
			  PDP_INTSTAT_INTS_VBLNK0_MASK)) {
		pdp_wreg32(pdp_reg, PDP_INTCLR_OFFSET,
			   (1 << PDP_INTCLR_INTCLR_VBLNK0_SHIFT));
		return true;
	}

	return false;
}

void pdp_plato_set_plane_enabled(struct device *dev, void __iomem *pdp_reg,
				 u32 plane, bool enable)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev, "Set plane %u: %s\n",
		 plane, enable ? "enable" : "disable");
#endif
	value = pdp_rreg32(pdp_reg, PDP_GRPH1CTRL_OFFSET);
	value = REG_VALUE_SET(value, enable ? 0x1 : 0x0,
			      PDP_GRPH1CTRL_GRPH1STREN_SHIFT,
			      PDP_GRPH1CTRL_GRPH1STREN_MASK);
	pdp_wreg32(pdp_reg, PDP_GRPH1CTRL_OFFSET, value);
}

void pdp_plato_set_surface(struct device *dev,
			   void __iomem *pdp_reg, void __iomem *pdp_bif_reg,
			   u32 plane, u64 address,
			   u32 posx, u32 posy,
			   u32 width, u32 height, u32 stride,
			   u32 format, u32 alpha, bool blend)
{
	u32 value;

#ifdef PDP_VERBOSE
	dev_info(dev,
		 "Set surface: size=%dx%d stride=%d format=%d address=0x%llx\n",
		 width, height, stride, format, address);
#endif

	pdp_wreg32(pdp_reg, PDP_REGISTER_UPDATE_CTRL_OFFSET, 0x0);
	/*
	 * Set the offset position to (0,0) as we've already added any offset
	 * to the base address.
	 */
	pdp_wreg32(pdp_reg, PDP_GRPH1POSN_OFFSET, 0);

	/* Set the frame buffer base address */
	if (address & 0xF)
		dev_warn(dev, "The frame buffer address is not aligned\n");

	pdp_wreg32(pdp_reg, PDP_GRPH1BASEADDR_OFFSET,
		   (u32)address & PDP_GRPH1BASEADDR_GRPH1BASEADDR_MASK);

	/*
	 * Write 8 msb of the address to address extension bits in the PDP
	 * MMU control register.
	 */
	value = pdp_rreg32(pdp_bif_reg, PDP_BIF_ADDRESS_CONTROL_OFFSET);
	value = REG_VALUE_SET(value, address >> 32,
			      PDP_BIF_ADDRESS_CONTROL_UPPER_ADDRESS_FIXED_SHIFT,
			      PDP_BIF_ADDRESS_CONTROL_UPPER_ADDRESS_FIXED_MASK);
	value = REG_VALUE_SET(value, 0x00,
			      PDP_BIF_ADDRESS_CONTROL_MMU_ENABLE_EXT_ADDRESSING_SHIFT,
			      PDP_BIF_ADDRESS_CONTROL_MMU_ENABLE_EXT_ADDRESSING_MASK);
	value = REG_VALUE_SET(value, 0x01,
			      PDP_BIF_ADDRESS_CONTROL_MMU_BYPASS_SHIFT,
			      PDP_BIF_ADDRESS_CONTROL_MMU_BYPASS_MASK);
	pdp_wreg32(pdp_bif_reg, PDP_BIF_ADDRESS_CONTROL_OFFSET, value);

	/* Set the framebuffer pixel format */
	value = pdp_rreg32(pdp_reg, PDP_GRPH1SURF_OFFSET);
	value = REG_VALUE_SET(value, format,
			      PDP_GRPH1SURF_GRPH1PIXFMT_SHIFT,
			      PDP_GRPH1SURF_GRPH1PIXFMT_MASK);
	pdp_wreg32(pdp_reg, PDP_GRPH1SURF_OFFSET, value);
	/*
	 * Set the framebuffer size (this might be smaller than the resolution)
	 */
	value = REG_VALUE_SET(0, width - 1,
			      PDP_GRPH1SIZE_GRPH1WIDTH_SHIFT,
			      PDP_GRPH1SIZE_GRPH1WIDTH_MASK);
	value = REG_VALUE_SET(value, height - 1,
			      PDP_GRPH1SIZE_GRPH1HEIGHT_SHIFT,
			      PDP_GRPH1SIZE_GRPH1HEIGHT_MASK);
	pdp_wreg32(pdp_reg, PDP_GRPH1SIZE_OFFSET, value);

	/* Set the framebuffer stride in 16byte words */
	value = REG_VALUE_SET(0, (stride >> PLATO_PDP_STRIDE_SHIFT) - 1,
			      PDP_GRPH1STRIDE_GRPH1STRIDE_SHIFT,
			      PDP_GRPH1STRIDE_GRPH1STRIDE_MASK);
	pdp_wreg32(pdp_reg, PDP_GRPH1STRIDE_OFFSET, value);

	/* Enable the register writes on the next vblank */
	pdp_wreg32(pdp_reg, PDP_REGISTER_UPDATE_CTRL_OFFSET, 0x3);

	/*
	 * Issues with NoC sending interleaved read responses to PDP require
	 * burst to be 1.
	 */
	value = REG_VALUE_SET(0, 0x02,
			      PDP_MEMCTRL_MEMREFRESH_SHIFT,
			      PDP_MEMCTRL_MEMREFRESH_MASK);
	value = REG_VALUE_SET(value, 0x01,
			      PDP_MEMCTRL_BURSTLEN_SHIFT,
			      PDP_MEMCTRL_BURSTLEN_MASK);
	pdp_wreg32(pdp_reg, PDP_MEMCTRL_OFFSET, value);
}

void pdp_plato_mode_set(struct device *dev, void __iomem *pdp_reg,
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

	/* Update control */
	value = pdp_rreg32(pdp_reg, PDP_REGISTER_UPDATE_CTRL_OFFSET);
	value = REG_VALUE_SET(value, 0x0,
			      PDP_REGISTER_UPDATE_CTRL_REGISTERS_VALID_SHIFT,
			      PDP_REGISTER_UPDATE_CTRL_REGISTERS_VALID_MASK);
	pdp_wreg32(pdp_reg, PDP_REGISTER_UPDATE_CTRL_OFFSET, value);

	/* Set hsync timings */
	value = pdp_rreg32(pdp_reg, PDP_HSYNC1_OFFSET);
	value = REG_VALUE_SET(value, hbps,
			      PDP_HSYNC1_HBPS_SHIFT,
			      PDP_HSYNC1_HBPS_MASK);
	value = REG_VALUE_SET(value, ht,
			      PDP_HSYNC1_HT_SHIFT,
			      PDP_HSYNC1_HT_MASK);
	pdp_wreg32(pdp_reg, PDP_HSYNC1_OFFSET, value);

	value = pdp_rreg32(pdp_reg, PDP_HSYNC2_OFFSET);
	value = REG_VALUE_SET(value, has,
			      PDP_HSYNC2_HAS_SHIFT,
			      PDP_HSYNC2_HAS_MASK);
	value = REG_VALUE_SET(value, hlbs,
			      PDP_HSYNC2_HLBS_SHIFT,
			      PDP_HSYNC2_HLBS_MASK);
	pdp_wreg32(pdp_reg, PDP_HSYNC2_OFFSET, value);

	value = pdp_rreg32(pdp_reg, PDP_HSYNC3_OFFSET);
	value = REG_VALUE_SET(value, hfps,
			      PDP_HSYNC3_HFPS_SHIFT,
			      PDP_HSYNC3_HFPS_MASK);
	value = REG_VALUE_SET(value, hrbs,
			      PDP_HSYNC3_HRBS_SHIFT,
			      PDP_HSYNC3_HRBS_MASK);
	pdp_wreg32(pdp_reg, PDP_HSYNC3_OFFSET, value);

	/* Set vsync timings */
	value = pdp_rreg32(pdp_reg, PDP_VSYNC1_OFFSET);
	value = REG_VALUE_SET(value, vbps,
			      PDP_VSYNC1_VBPS_SHIFT,
			      PDP_VSYNC1_VBPS_MASK);
	value = REG_VALUE_SET(value, vt,
			      PDP_VSYNC1_VT_SHIFT,
			      PDP_VSYNC1_VT_MASK);
	pdp_wreg32(pdp_reg, PDP_VSYNC1_OFFSET, value);

	value = pdp_rreg32(pdp_reg, PDP_VSYNC2_OFFSET);
	value = REG_VALUE_SET(value, vas,
			      PDP_VSYNC2_VAS_SHIFT,
			      PDP_VSYNC2_VAS_MASK);
	value = REG_VALUE_SET(value, vtbs,
			      PDP_VSYNC2_VTBS_SHIFT,
			      PDP_VSYNC2_VTBS_MASK);
	pdp_wreg32(pdp_reg, PDP_VSYNC2_OFFSET, value);

	value = pdp_rreg32(pdp_reg, PDP_VSYNC3_OFFSET);
	value = REG_VALUE_SET(value, vfps,
			      PDP_VSYNC3_VFPS_SHIFT,
			      PDP_VSYNC3_VFPS_MASK);
	value = REG_VALUE_SET(value, vbbs,
			      PDP_VSYNC3_VBBS_SHIFT,
			      PDP_VSYNC3_VBBS_MASK);
	pdp_wreg32(pdp_reg, PDP_VSYNC3_OFFSET, value);

	/* Horizontal data enable */
	value = pdp_rreg32(pdp_reg, PDP_HDECTRL_OFFSET);
	value = REG_VALUE_SET(value, has,
			      PDP_HDECTRL_HDES_SHIFT,
			      PDP_HDECTRL_HDES_MASK);
	value = REG_VALUE_SET(value, hrbs,
			      PDP_HDECTRL_HDEF_SHIFT,
			      PDP_HDECTRL_HDEF_MASK);
	pdp_wreg32(pdp_reg, PDP_HDECTRL_OFFSET, value);

	/* Vertical data enable */
	value = pdp_rreg32(pdp_reg, PDP_VDECTRL_OFFSET);
	value = REG_VALUE_SET(value, vtbs, /* XXX: we're setting this to VAS */
			      PDP_VDECTRL_VDES_SHIFT,
			      PDP_VDECTRL_VDES_MASK);
	value = REG_VALUE_SET(value, vfps, /* XXX: set to VBBS */
			      PDP_VDECTRL_VDEF_SHIFT,
			      PDP_VDECTRL_VDEF_MASK);
	pdp_wreg32(pdp_reg, PDP_VDECTRL_OFFSET, value);

	/* Vertical event start and vertical fetch start */
	value = 0;
	value = REG_VALUE_SET(value, 0,
			      PDP_VEVENT_VEVENT_SHIFT,
			      PDP_VEVENT_VEVENT_MASK);
	value = REG_VALUE_SET(value, vbps,
			      PDP_VEVENT_VFETCH_SHIFT,
			      PDP_VEVENT_VFETCH_MASK);
	value = REG_VALUE_SET(value, vfps,
			      PDP_VEVENT_VEVENT_SHIFT,
			      PDP_VEVENT_VEVENT_MASK);
	pdp_wreg32(pdp_reg, PDP_VEVENT_OFFSET, value);

	/* Set up polarities of sync/blank */
	value = REG_VALUE_SET(0, 0x1,
			      PDP_SYNCCTRL_BLNKPOL_SHIFT,
			      PDP_SYNCCTRL_BLNKPOL_MASK);

	if (nhsync)
		value = REG_VALUE_SET(value, 0x1,
				      PDP_SYNCCTRL_HSPOL_SHIFT,
				      PDP_SYNCCTRL_HSPOL_MASK);

	if (nvsync)
		value = REG_VALUE_SET(value, 0x1,
				      PDP_SYNCCTRL_VSPOL_SHIFT,
				      PDP_SYNCCTRL_VSPOL_MASK);

	pdp_wreg32(pdp_reg,
		   PDP_SYNCCTRL_OFFSET,
		   value);
}
