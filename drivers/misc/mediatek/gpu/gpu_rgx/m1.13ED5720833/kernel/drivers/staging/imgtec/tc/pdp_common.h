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

#if !defined(__PDP_COMMON_H__)
#define __PDP_COMMON_H__

#include <linux/io.h>

/*#define PDP_VERBOSE*/

#define REG_VALUE_GET(v, s, m) \
	(u32)(((v) & (m)) >> (s))
#define REG_VALUE_SET(v, b, s, m) \
	(u32)(((v) & (u32)~(m)) | (u32)(((b) << (s)) & (m)))
/* Active low */
#define REG_VALUE_LO(v, b, s, m) \
	(u32)((v) & ~(u32)(((b) << (s)) & (m)))

enum pdp_version {
	PDP_VERSION_APOLLO,
	PDP_VERSION_ODIN,
	PDP_VERSION_PLATO,
};

enum pdp_odin_subversion {
	PDP_ODIN_NONE = 0,
	PDP_ODIN_ORION,
};

/* Register R-W */
static inline u32 core_rreg32(void __iomem *base, resource_size_t reg)
{
	return ioread32(base + reg);
}

static inline void core_wreg32(void __iomem *base, resource_size_t reg,
			       u32 value)
{
	iowrite32(value, base + reg);
}

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

#endif /* __PDP_COMMON_H__ */
