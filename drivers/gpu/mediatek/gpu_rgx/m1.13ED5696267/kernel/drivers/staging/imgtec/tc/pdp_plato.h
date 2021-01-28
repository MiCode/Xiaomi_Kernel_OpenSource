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

#if !defined(__PDP_PLATO_H__)
#define __PDP_PLATO_H__

#include <linux/device.h>
#include <linux/types.h>

#define PLATO_PDP_PIXEL_FORMAT_G        (0x00)
#define PLATO_PDP_PIXEL_FORMAT_ARGB4    (0x04)
#define PLATO_PDP_PIXEL_FORMAT_ARGB1555 (0x05)
#define PLATO_PDP_PIXEL_FORMAT_RGB8     (0x06)
#define PLATO_PDP_PIXEL_FORMAT_RGB565   (0x07)
#define PLATO_PDP_PIXEL_FORMAT_ARGB8    (0x08)
#define PLATO_PDP_PIXEL_FORMAT_AYUV8    (0x10)
#define PLATO_PDP_PIXEL_FORMAT_YUV10    (0x15)
#define PLATO_PDP_PIXEL_FORMAT_RGBA8    (0x16)


void pdp_plato_set_syncgen_enabled(struct device *dev, void __iomem *pdp_reg,
				   bool enable);

void pdp_plato_set_vblank_enabled(struct device *dev, void __iomem *pdp_reg,
				  bool enable);

bool pdp_plato_check_and_clear_vblank(struct device *dev,
				      void __iomem *pdp_reg);

void pdp_plato_set_plane_enabled(struct device *dev, void __iomem *pdp_reg,
				 u32 plane, bool enable);

void pdp_plato_set_surface(struct device *dev,
			   void __iomem *pdp_reg, void __iomem *pdp_bif_reg,
			   u32 plane, u64 address,
			   u32 posx, u32 posy,
			   u32 width, u32 height, u32 stride,
			   u32 format, u32 alpha, bool blend);

void pdp_plato_mode_set(struct device *dev, void __iomem *pdp_reg,
			u32 h_display, u32 v_display,
			u32 hbps, u32 ht, u32 has,
			u32 hlbs, u32 hfps, u32 hrbs,
			u32 vbps, u32 vt, u32 vas,
			u32 vtbs, u32 vfps, u32 vbbs,
			bool nhsync, bool nvsync);

#endif /* __PDP_PLATO_H__ */
