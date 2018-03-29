/*************************************************************************/ /*!
@File
@Title          RGX Common Types and Defines Header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description	Common types and definitions for RGX software
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
#ifndef RGX_COMMON_H_
#define RGX_COMMON_H_

#if defined (__cplusplus)
extern "C" {
#endif

#include "img_defs.h"

/* Included to get the BVNC_KM_N defined and other feature defs */
#include "km/rgxdefs_km.h"

/*! This macro represents a mask of LSBs that must be zero on data structure
 * sizes and offsets to ensure they are 8-byte granular on types shared between
 * the FW and host driver */
#define RGX_FW_ALIGNMENT_LSB (7)

/*! Macro to test structure size alignment */
#define RGX_FW_STRUCT_SIZE_ASSERT(_a)	\
	static_assert((sizeof(_a) & RGX_FW_ALIGNMENT_LSB) == 0,	\
				  "Size of " #_a " is not properly aligned")

/*! Macro to test structure member alignment */
#define RGX_FW_STRUCT_OFFSET_ASSERT(_a, _b)	\
	static_assert((offsetof(_a, _b) & RGX_FW_ALIGNMENT_LSB) == 0,	\
				  "Offset of " #_a "." #_b " is not properly aligned")


/* The following enum assumes only one of RGX_FEATURE_TLA or RGX_FEATURE_FASTRENDER_DM feature
 * is present. In case this is no more true, fail build to fix code */
#if defined (RGX_FEATURE_TLA) && defined (RGX_FEATURE_FASTRENDER_DM)
#error "Both RGX_FEATURE_TLA and RGX_FEATURE_FASTRENDER_DM defined. Fix code to handle this!"
#endif

/*! The master definition for data masters known to the firmware of RGX.
 * When a new DM is added to this enum, relevant entry should be added to
 * RGX_HWPERF_DM enum list.
 * The DM in a V1 HWPerf packet uses this definition. */
typedef enum _RGXFWIF_DM_
{
	RGXFWIF_DM_GP			= 0,
#if defined(RGX_FEATURE_TLA)
	RGXFWIF_DM_2D			= 1,
#endif
#if defined(RGX_FEATURE_FASTRENDER_DM)
	RGXFWIF_DM_TDM			= 1,
#endif
	RGXFWIF_DM_TA			= 2,
	RGXFWIF_DM_3D			= 3,
	RGXFWIF_DM_CDM			= 4,
#if defined(RGX_FEATURE_RAY_TRACING)
	RGXFWIF_DM_RTU			= 5,
	RGXFWIF_DM_SHG			= 6,
#endif

	RGXFWIF_DM_LAST,

	RGXFWIF_DM_FORCE_I32  = 0x7fffffff   /*!< Force enum to be at least 32-bits wide */
} RGXFWIF_DM;

#if defined(RGX_FEATURE_RAY_TRACING)
#define RGXFWIF_DM_MAX_MTS 8
#else
#define RGXFWIF_DM_MAX_MTS 6
#endif

#if defined(RGX_FEATURE_RAY_TRACING)
/* Maximum number of DM in use: GP, 2D/TDM, TA, 3D, CDM, SHG, RTU */
#define RGXFWIF_DM_MAX			(7)
#else
#define RGXFWIF_DM_MAX			(5)
#endif

/* Min/Max number of HW DMs (all but GP) */
#if defined(RGX_FEATURE_TLA)
#define RGXFWIF_HWDM_MIN		(1)
#else
#if defined(RGX_FEATURE_FASTRENDER_DM)
#define RGXFWIF_HWDM_MIN		(1)
#else
#define RGXFWIF_HWDM_MIN		(2)
#endif
#endif
#define RGXFWIF_HWDM_MAX		(RGXFWIF_DM_MAX)

/*!
 ******************************************************************************
 * RGXFW Compiler alignment definitions
 *****************************************************************************/
#if defined(__GNUC__) || defined(HAS_GNUC_ATTRIBUTES)
#define RGXFW_ALIGN			__attribute__ ((aligned (8)))
#elif defined(_MSC_VER)
#define RGXFW_ALIGN			__declspec(align(8))
#pragma warning (disable : 4324)
#else
#error "Align MACROS need to be defined for this compiler"
#endif

/*!
 ******************************************************************************
 * Force 8-byte alignment for structures allocated uncached.
 *****************************************************************************/
#define UNCACHED_ALIGN      RGXFW_ALIGN

#if defined (__cplusplus)
}
#endif

#endif /* RGX_COMMON_H_ */

/******************************************************************************
 End of file
******************************************************************************/

