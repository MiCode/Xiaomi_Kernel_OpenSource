/*************************************************************************/ /*!
@File
@Title          RGX Bridge Functionality
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Header for the rgx Bridge code
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

#if !defined(__RGX_BRIDGE_H__)
#define __RGX_BRIDGE_H__

#include "pvr_bridge.h"

#if defined (__cplusplus)
extern "C" {
#endif

#if !defined(SUPPORT_KERNEL_SRVINIT)
#include "common_rgxinit_bridge.h"
#endif
#include "common_rgxta3d_bridge.h"
#if defined(RGX_FEATURE_COMPUTE)
#include "common_rgxcmp_bridge.h"
#endif
#include "common_rgxtq2_bridge.h"
#include "common_rgxtq_bridge.h"
#if !defined(EXCLUDE_BREAKPOINT_BRIDGE)
#include "common_breakpoint_bridge.h"
#endif
#include "common_debugmisc_bridge.h"
#if defined(PDUMP)
#include "common_rgxpdump_bridge.h"
#endif
#include "common_rgxhwperf_bridge.h"
#if defined(RGX_FEATURE_RAY_TRACING)
#include "common_rgxray_bridge.h"
#endif
#if !defined(EXCLUDE_REGCONFIG_BRIDGE)
#include "common_regconfig_bridge.h"
#endif
#include "common_timerquery_bridge.h"
#include "common_rgxkicksync_bridge.h"
#if defined(RGX_FEATURE_SIGNAL_SNOOPING)
#include "common_rgxsignals_bridge.h"
#endif

/*
 * Bridge Cmd Ids
 */

/* *REMEMBER* to update PVRSRV_BRIDGE_RGX_LAST if you add/remove a bridge
 * group!
 * Also you need to ensure all PVRSRV_BRIDGE_RGX_xxx_DISPATCH_FIRST
 * offsets follow on from the previous bridge group's commands!
 *
 * If a bridge group is optional, ensure you *ALWAYS* define its index
 * (e.g. PVRSRV_BRIDGE_RGXCMP is always 151, even is the feature is
 * not defined). If an optional bridge group is not defined you must
 * still define PVRSRV_BRIDGE_RGX_xxx_DISPATCH_FIRST for it with an
 * assigned value of 0.
 */

/* The RGX bridge groups start at 128 rather than follow-on from the other
 * non-device bridge groups (meaning that they then won't be displaced if
 * other non-device bridge groups are added)
 */

/* 128: RGX TQ interface functions */
#define PVRSRV_BRIDGE_RGXTQ                      128UL
#define PVRSRV_BRIDGE_RGXTQ_DISPATCH_FIRST       (PVRSRV_BRIDGE_DISPATCH_LAST + 1)
#define PVRSRV_BRIDGE_RGXTQ_DISPATCH_LAST        (PVRSRV_BRIDGE_RGXTQ_DISPATCH_FIRST + PVRSRV_BRIDGE_RGXTQ_CMD_LAST)


/* 129: RGX Compute interface functions */
#define PVRSRV_BRIDGE_RGXCMP                     129UL
#if defined(RGX_FEATURE_COMPUTE)
#	define PVRSRV_BRIDGE_RGXCMP_DISPATCH_FIRST   (PVRSRV_BRIDGE_RGXTQ_DISPATCH_LAST + 1)
#	define PVRSRV_BRIDGE_RGXCMP_DISPATCH_LAST    (PVRSRV_BRIDGE_RGXCMP_DISPATCH_FIRST + PVRSRV_BRIDGE_RGXCMP_CMD_LAST)
#else
#	define PVRSRV_BRIDGE_RGXCMP_DISPATCH_FIRST   0
#	define PVRSRV_BRIDGE_RGXCMP_DISPATCH_LAST    (PVRSRV_BRIDGE_RGXTQ_DISPATCH_LAST)
#endif

/* 130: RGX Initialisation interface functions */
#define PVRSRV_BRIDGE_RGXINIT                    130UL
#if !defined(SUPPORT_KERNEL_SRVINIT)
#	define PVRSRV_BRIDGE_RGXINIT_DISPATCH_FIRST  (PVRSRV_BRIDGE_RGXCMP_DISPATCH_LAST +1)
#	define PVRSRV_BRIDGE_RGXINIT_DISPATCH_LAST   (PVRSRV_BRIDGE_RGXINIT_DISPATCH_FIRST + PVRSRV_BRIDGE_RGXINIT_CMD_LAST)
#else
#	define PVRSRV_BRIDGE_RGXINIT_DISPATCH_FIRST  0
#	define PVRSRV_BRIDGE_RGXINIT_DISPATCH_LAST   (PVRSRV_BRIDGE_RGXCMP_DISPATCH_LAST)
#endif

/* 131: RGX TA/3D interface functions */
#define PVRSRV_BRIDGE_RGXTA3D                    131UL
#define PVRSRV_BRIDGE_RGXTA3D_DISPATCH_FIRST     (PVRSRV_BRIDGE_RGXINIT_DISPATCH_LAST +1)
#define PVRSRV_BRIDGE_RGXTA3D_DISPATCH_LAST      (PVRSRV_BRIDGE_RGXTA3D_DISPATCH_FIRST + PVRSRV_BRIDGE_RGXTA3D_CMD_LAST)

/* 132: RGX Breakpoint interface functions */
#define PVRSRV_BRIDGE_BREAKPOINT                 132UL
#if !defined(EXCLUDE_BREAKPOINT_BRIDGE)
#define PVRSRV_BRIDGE_BREAKPOINT_DISPATCH_FIRST  (PVRSRV_BRIDGE_RGXTA3D_DISPATCH_LAST + 1)
#define PVRSRV_BRIDGE_BREAKPOINT_DISPATCH_LAST   (PVRSRV_BRIDGE_BREAKPOINT_DISPATCH_FIRST + PVRSRV_BRIDGE_BREAKPOINT_CMD_LAST)
#else
#define PVRSRV_BRIDGE_BREAKPOINT_DISPATCH_FIRST  0
#define PVRSRV_BRIDGE_BREAKPOINT_DISPATCH_LAST   (PVRSRV_BRIDGE_RGXTA3D_DISPATCH_LAST)
#endif

/* 133: RGX Debug/Misc interface functions */
#define PVRSRV_BRIDGE_DEBUGMISC                  133UL
#define PVRSRV_BRIDGE_DEBUGMISC_DISPATCH_FIRST   (PVRSRV_BRIDGE_BREAKPOINT_DISPATCH_LAST + 1)
#define PVRSRV_BRIDGE_DEBUGMISC_DISPATCH_LAST    (PVRSRV_BRIDGE_DEBUGMISC_DISPATCH_FIRST + PVRSRV_BRIDGE_DEBUGMISC_CMD_LAST)

/* 134: RGX PDump interface functions */
#define PVRSRV_BRIDGE_RGXPDUMP                   134UL
#if defined(PDUMP)
#define PVRSRV_BRIDGE_RGXPDUMP_DISPATCH_FIRST    (PVRSRV_BRIDGE_DEBUGMISC_DISPATCH_LAST +1)
#define PVRSRV_BRIDGE_RGXPDUMP_DISPATCH_LAST     (PVRSRV_BRIDGE_RGXPDUMP_DISPATCH_FIRST + PVRSRV_BRIDGE_RGXPDUMP_CMD_LAST)
#else
#define PVRSRV_BRIDGE_RGXPDUMP_DISPATCH_FIRST    0
#define PVRSRV_BRIDGE_RGXPDUMP_DISPATCH_LAST     (PVRSRV_BRIDGE_DEBUGMISC_DISPATCH_LAST)
#endif

/* 135: RGX HWPerf interface functions */
#define PVRSRV_BRIDGE_RGXHWPERF                  135UL
#define PVRSRV_BRIDGE_RGXHWPERF_DISPATCH_FIRST   (PVRSRV_BRIDGE_RGXPDUMP_DISPATCH_LAST + 1)
#define PVRSRV_BRIDGE_RGXHWPERF_DISPATCH_LAST    (PVRSRV_BRIDGE_RGXHWPERF_DISPATCH_FIRST + PVRSRV_BRIDGE_RGXHWPERF_CMD_LAST)

/* 136: RGX Ray Tracing interface functions */
#define PVRSRV_BRIDGE_RGXRAY                     136UL
#if defined(RGX_FEATURE_RAY_TRACING)
#define PVRSRV_BRIDGE_RGXRAY_DISPATCH_FIRST      (PVRSRV_BRIDGE_RGXHWPERF_DISPATCH_LAST + 1)
#define PVRSRV_BRIDGE_RGXRAY_DISPATCH_LAST       (PVRSRV_BRIDGE_RGXRAY_DISPATCH_FIRST + PVRSRV_BRIDGE_RGXRAY_CMD_LAST)
#else
#define PVRSRV_BRIDGE_RGXRAY_DISPATCH_FIRST      0
#define PVRSRV_BRIDGE_RGXRAY_DISPATCH_LAST       (PVRSRV_BRIDGE_RGXHWPERF_DISPATCH_LAST)
#endif

/* 137: RGX Register Configuration interface functions */
#define PVRSRV_BRIDGE_REGCONFIG                  137UL
#if !defined(EXCLUDE_REGCONFIG_BRIDGE)
#define PVRSRV_BRIDGE_REGCONFIG_DISPATCH_FIRST   (PVRSRV_BRIDGE_RGXRAY_DISPATCH_LAST + 1)
#define PVRSRV_BRIDGE_REGCONFIG_DISPATCH_LAST    (PVRSRV_BRIDGE_REGCONFIG_DISPATCH_FIRST + PVRSRV_BRIDGE_REGCONFIG_CMD_LAST)
#else
#define PVRSRV_BRIDGE_REGCONFIG_DISPATCH_FIRST   0
#define PVRSRV_BRIDGE_REGCONFIG_DISPATCH_LAST    (PVRSRV_BRIDGE_RGXRAY_DISPATCH_LAST)
#endif

/* 138: RGX Timer Query interface functions */
#define PVRSRV_BRIDGE_TIMERQUERY                 138UL
#define PVRSRV_BRIDGE_TIMERQUERY_DISPATCH_FIRST  (PVRSRV_BRIDGE_REGCONFIG_DISPATCH_LAST + 1)
#define PVRSRV_BRIDGE_TIMERQUERY_DISPATCH_LAST   (PVRSRV_BRIDGE_TIMERQUERY_DISPATCH_FIRST + PVRSRV_BRIDGE_TIMERQUERY_CMD_LAST)

/* 139: RGX kicksync interface */
#define PVRSRV_BRIDGE_RGXKICKSYNC                139UL
#define PVRSRV_BRIDGE_RGXKICKSYNC_DISPATCH_FIRST (PVRSRV_BRIDGE_TIMERQUERY_DISPATCH_LAST + 1)
#define PVRSRV_BRIDGE_RGXKICKSYNC_DISPATCH_LAST  (PVRSRV_BRIDGE_RGXKICKSYNC_DISPATCH_FIRST + PVRSRV_BRIDGE_RGXKICKSYNC_CMD_LAST)

/* 140: RGX signals interface */
#define PVRSRV_BRIDGE_RGXSIGNALS                140UL
#if defined(RGX_FEATURE_SIGNAL_SNOOPING)
#define PVRSRV_BRIDGE_RGXSIGNALS_DISPATCH_FIRST (PVRSRV_BRIDGE_RGXKICKSYNC_DISPATCH_LAST + 1)
#define PVRSRV_BRIDGE_RGXSIGNALS_DISPATCH_LAST  (PVRSRV_BRIDGE_RGXSIGNALS_DISPATCH_FIRST + PVRSRV_BRIDGE_RGXSIGNALS_CMD_LAST)
#else
#define PVRSRV_BRIDGE_RGXSIGNALS_DISPATCH_FIRST  0
#define PVRSRV_BRIDGE_RGXSIGNALS_DISPATCH_LAST   (PVRSRV_BRIDGE_RGXKICKSYNC_DISPATCH_LAST)
#endif

#define PVRSRV_BRIDGE_RGXTQ2                      141UL
#define PVRSRV_BRIDGE_RGXTQ2_DISPATCH_FIRST       (PVRSRV_BRIDGE_RGXSIGNALS_DISPATCH_LAST + 1)
#define PVRSRV_BRIDGE_RGXTQ2_DISPATCH_LAST        (PVRSRV_BRIDGE_RGXTQ2_DISPATCH_FIRST + PVRSRV_BRIDGE_RGXTQ2_CMD_LAST)

#define PVRSRV_BRIDGE_RGX_LAST                   (PVRSRV_BRIDGE_RGXTQ2)
#define PVRSRV_BRIDGE_RGX_DISPATCH_LAST          (PVRSRV_BRIDGE_RGXTQ2_DISPATCH_LAST)

#if defined (__cplusplus)
}
#endif

#endif /* __RGX_BRIDGE_H__ */
