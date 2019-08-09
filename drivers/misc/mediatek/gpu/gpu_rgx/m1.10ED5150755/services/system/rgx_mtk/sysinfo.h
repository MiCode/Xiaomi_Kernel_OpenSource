/*************************************************************************/ /*!
 *
File
Title          System Description Header
Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
Description    This header provides system-specific declarations and macros
License        Dual MIT/GPLv2

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

#if !defined(__SYSINFO_H__)
#define __SYSINFO_H__

/*!< System specific poll/timeout details */
#if defined(PVR_LINUX_USING_WORKQUEUES)
#define MAX_HW_TIME_US					(1000000)
#define DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT		(10000)
#define DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT	(3600000)
#define WAIT_TRY_COUNT					(20000)
#else
#define MAX_HW_TIME_US					(5000000)
#define DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT		(10000)
#define DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT	(3600000)
#define WAIT_TRY_COUNT					(100000)
#endif

/* RGX, DISPLAY (external), BUFFER (external) */
#define SYS_DEVICE_COUNT		3

#define SYS_PHYS_HEAP_COUNT		1

#if defined(CONFIG_MACH_MT8173)
#define SYS_RGX_OF_COMPATIBLE	"mediatek,mt8173-han"
#elif defined(CONFIG_MACH_MT8167)
#define SYS_RGX_OF_COMPATIBLE	"mediatek,mt8167-clark"
#elif defined(CONFIG_MACH_MT6739)
#define SYS_RGX_OF_COMPATIBLE	"mediatek,AUCKLAND"
#elif defined(CONFIG_MACH_MT6765)
#define SYS_RGX_OF_COMPATIBLE	"mediatek,doma"
#elif defined(CONFIG_MACH_MT6779)
#define SYS_RGX_OF_COMPATIBLE	"mediatek,lorne"
#else
#endif

#if defined(__linux__)
/*
 * Use the static bus ID for the platform DRM device.
 */
#if defined(PVR_DRM_DEV_BUS_ID)
#define	SYS_RGX_DEV_DRM_BUS_ID	PVR_DRM_DEV_BUS_ID
#else
#define SYS_RGX_DEV_DRM_BUS_ID	"platform:pvrsrvkm"
#endif	/* defined(PVR_DRM_DEV_BUS_ID) */
#endif

#endif	/* !defined(__SYSINFO_H__) */
