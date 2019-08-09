/*************************************************************************/ /*!
 *
@Title          System Description Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    This header provides system-specific declarations and macros
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

#include "pvrsrv_device.h"
#include "rgxdevice.h"

#if !defined(__SYSCCONFIG_H__)
#define __SYSCCONFIG_H__


#define RGX_HW_SYSTEM_NAME "RGX HW"

#if defined(CONFIG_MACH_MT8173)
#define RGX_HW_CORE_CLOCK_SPEED			(455000000)
#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (30)
#elif defined(CONFIG_MACH_MT8167)
#define RGX_HW_CORE_CLOCK_SPEED			(500000000)
#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (50)
#elif defined(CONFIG_MACH_MT6739)
#define RGX_HW_CORE_CLOCK_SPEED			(481000000)
#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (3)
#elif defined(CONFIG_MACH_MT6765)
#define RGX_HW_CORE_CLOCK_SPEED			(400000000)
#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (3)
#elif defined(CONFIG_MACH_MT6761)
#define RGX_HW_CORE_CLOCK_SPEED			(460000000)
#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (3)
#elif defined(CONFIG_MACH_MT6779)
#define RGX_HW_CORE_CLOCK_SPEED			(100000000)
#define SYS_RGX_ACTIVE_POWER_LATENCY_MS (3)
#else
#endif

static IMG_UINT32 gauiBIFTilingHeapXStrides[RGXFWIF_NUM_BIF_TILING_CONFIGS] = {
	0, /* BIF tiling heap 1 x-stride */
	1, /* BIF tiling heap 2 x-stride */
	2, /* BIF tiling heap 3 x-stride */
	3  /* BIF tiling heap 4 x-stride */
};

#if defined(MTK_CONFIG_OF) && defined(CONFIG_OF)
int MTKSysGetIRQ(void);
#else

/* If CONFIG_OF is not set */
/* please makesure the following address and IRQ number are right */
/* #error RGX_GPU_please_fill_the_following_defines */
#define SYS_MTK_RGX_REGS_SYS_PHYS_BASE      0x13000000
#define SYS_MTK_RGX_REGS_SIZE               0x80000

#if defined(CONFIG_MACH_MT8173)
#define SYS_MTK_RGX_IRQ                     0x102
#elif defined(CONFIG_MACH_MT8167)
#define SYS_MTK_RGX_IRQ                     0xDB
#elif defined(CONFIG_MACH_MT6739)
#define SYS_MTK_RGX_IRQ                     0x150
#elif defined(CONFIG_MACH_MT6765)
#define SYS_MTK_RGX_IRQ                     0x103
#elif defined(CONFIG_MACH_MT6779)
#define SYS_MTK_RGX_IRQ                     0x130
#else
#endif

#endif



/*****************************************************************************
 * system specific data structures
 *****************************************************************************/

#endif	/* __SYSCCONFIG_H__ */
