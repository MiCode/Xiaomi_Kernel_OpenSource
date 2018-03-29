/*************************************************************************/ /*!
@File
@Title          RGX HW Performance counter table
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    RGX HW Performance counters table
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
#include "rgx_fwif_hwperf.h"
#include "rgxdefs_km.h"
#include "rgx_hwperf_table.h"

/* Used for counter blocks that are always powered when FW is running */
#if defined(RGX_FEATURE_PERFBUS)
static IMG_BOOL rgxfw_hwperf_pow_st_true(RGX_HWPERF_CNTBLK_ID eBlkType, IMG_UINT8 ui8UnitId)
{
	PVR_UNREFERENCED_PARAMETER(eBlkType);
	PVR_UNREFERENCED_PARAMETER(ui8UnitId);

	return IMG_TRUE;
}
#endif

#if defined(RGX_FIRMWARE)

#	include "rgxfw_pow.h"
#	include "rgxfw_utils.h"

#if !defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE) && defined(RGX_FEATURE_PERFBUS)
static IMG_BOOL rgxfw_hwperf_pow_st_direct(RGX_HWPERF_CNTBLK_ID eBlkType, IMG_UINT8 ui8UnitId)
{
	PVR_UNREFERENCED_PARAMETER(eBlkType);
	PVR_UNREFERENCED_PARAMETER(ui8UnitId);
	
	/* Avoid unused function warning for S6 BVNCs */	
	PVR_UNREFERENCED_PARAMETER(rgxfw_hwperf_pow_st_true);

#if defined(RGX_FEATURE_XT_TOP_INFRASTRUCTURE)
	/* S6XT: TA, TORNADO */
	return IMG_TRUE;
#else
	/* S6  : TA, HUB, RASTER (RASCAL) */
	return (gsPowCtl.ePowState & RGXFW_POW_ST_RD_ON) ? IMG_TRUE : IMG_FALSE;
#endif
}
#endif /* !RGX_FEATURE_S7_TOP_INFRASTRUCTURE */


/* Only use conditional compilation when counter blocks appear in different
 * islands for different Rogue families.
 */
#if defined(RGX_FEATURE_PERFBUS)
static IMG_BOOL rgxfw_hwperf_pow_st_indirect(RGX_HWPERF_CNTBLK_ID eBlkType, IMG_UINT8 ui8UnitId)
{
	IMG_UINT32 ui32NumDustsEnabled = rgxfw_pow_get_enabled_dusts_num();

	if ((gsPowCtl.ePowState & RGXFW_POW_ST_RD_ON) &&
			(ui32NumDustsEnabled > 0))
	{
#if defined(RGX_FEATURE_DYNAMIC_DUST_POWER)
		IMG_UINT32 ui32NumUscEnabled = ui32NumDustsEnabled*2;

		switch (eBlkType)
		{
		case RGX_CNTBLK_ID_TPU_MCU0:                   /* S6 and S6XT */
#if defined (RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
		case RGX_CNTBLK_ID_TEXAS0:                     /* S7 */
#endif
			if (ui8UnitId >= ui32NumDustsEnabled)
			{
				return IMG_FALSE;
			}
			break;
		case RGX_CNTBLK_ID_USC0:                       /* S6, S6XT, S7 */
		case RGX_CNTBLK_ID_PBE0:                       /* S7 */
			/* Handle single cluster cores */
			if (ui8UnitId >= ((ui32NumUscEnabled > RGX_FEATURE_NUM_CLUSTERS) ? RGX_FEATURE_NUM_CLUSTERS : ui32NumUscEnabled))
			{
				return IMG_FALSE;
			}
			break;
		case RGX_CNTBLK_ID_BLACKPEARL0:                /* S7 */
		case RGX_CNTBLK_ID_RASTER0:                    /* S6XT */
#if defined(RGX_FEATURE_XT_TOP_INFRASTRUCTURE)
		case RGX_CNTBLK_ID_TEXAS0:                     /* S6XT */
#endif
			if (ui8UnitId >= (RGX_REQ_NUM_PHANTOMS(ui32NumUscEnabled)))
			{
				return IMG_FALSE;
			}
			break;
		default:
			RGXFW_ASSERT(IMG_FALSE);  /* should never get here, table error */
			break;
		}
#else
		/* Always true, no fused DUSTs, all powered so do not check unit */
		PVR_UNREFERENCED_PARAMETER(eBlkType);
		PVR_UNREFERENCED_PARAMETER(ui8UnitId);
#endif
	}
	else
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}
#endif
#else
#  define rgxfw_hwperf_pow_st_direct   rgxfw_hwperf_pow_st_true
#  define rgxfw_hwperf_pow_st_indirect rgxfw_hwperf_pow_st_true
#endif /* RGX_FIRMWARE */

#if defined(RGX_FEATURE_RAY_TRACING)
/* Currently there is no power island control in the firmware so 
 * we currently assume these blocks are always powered. */
#  define rgxfw_hwperf_pow_st_gandalf        rgxfw_hwperf_pow_st_true
#endif /* RGX_FEATURE_RAY_TRACING */

#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
/* A direct block only in S7, always powered as it contains Garten like
 * Sidekick (S6) and Tornado (S6XT). 
 */
#	define rgxfw_hwperf_pow_st_jones          rgxfw_hwperf_pow_st_true
#endif /* RGX_FEATURE_S7_TOP_INFRASTRUCTURE */

/* This tables holds the entries for the performance counter block type model 
 * Where the block is not present for a given feature then a NULL block must
 * be included to ensure index hash from a RGX_HWPERF_CNTBLK_ID remains valid */
static const RGXFW_HWPERF_CNTBLK_TYPE_MODEL gasCntBlkTypeModel[] =
{
/*   uiCntBlkIdBase,         iIndirectReg,                  uiPerfReg,                  uiSelect0BaseReg,                    uiBitSelectPreserveMask,                       uiNumCounters,  uiNumUnits,                    uiSelectRegModeShift, uiSelectRegOffsetShift,            pfnIsBlkPowered
 *                                                                                                                                    uiCounter0BaseReg                                                                                              pszBlockNameComment,  */
	/*RGX_CNTBLK_ID_TA*/
#if !defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE) && defined(RGX_FEATURE_PERFBUS)
    {RGX_CNTBLK_ID_TA,       0, /* direct */                RGX_CR_TA_PERF,             RGX_CR_TA_PERF_SELECT0,              0x0000,  RGX_CR_TA_PERF_COUNTER_0,             4,              1,                              21,                  3,  "RGX_CR_TA_PERF",              rgxfw_hwperf_pow_st_direct },
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_TA),
#endif

	/*RGX_CNTBLK_ID_RASTER*/
#if !defined(RGX_FEATURE_CLUSTER_GROUPING) && defined(RGX_FEATURE_PERFBUS)
    {RGX_CNTBLK_ID_RASTER,   0, /* direct */                RGX_CR_RASTERISATION_PERF,  RGX_CR_RASTERISATION_PERF_SELECT0,
#if defined(HW_ERN_44885)
                                                                                                                             0x7C00,
#else
                                                                                                                             0x0000,
#endif
                                                                                                                                      RGX_CR_RASTERISATION_PERF_COUNTER_0,  4,              1,                              21,                  3,  "RGX_CR_RASTERISATION_PERF",   rgxfw_hwperf_pow_st_direct },
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_RASTER),
#endif

	/*RGX_CNTBLK_ID_HUB*/
#if !defined(RGX_FEATURE_CLUSTER_GROUPING) && defined(RGX_FEATURE_PERFBUS)
	{RGX_CNTBLK_ID_HUB,      0, /* direct */                RGX_CR_HUB_BIFPMCACHE_PERF, RGX_CR_HUB_BIFPMCACHE_PERF_SELECT0,  0x0000,  RGX_CR_HUB_BIFPMCACHE_PERF_COUNTER_0, 4,              1,                              21,                  3,  "RGX_CR_HUB_BIFPMCACHE_PERF",  rgxfw_hwperf_pow_st_direct},
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_HUB),
#endif

	/*RGX_CNTBLK_ID_TORNADO*/
#if defined(RGX_FEATURE_XT_TOP_INFRASTRUCTURE)
    {RGX_CNTBLK_ID_TORNADO,  0, /* direct */                RGX_CR_TORNADO_PERF,        RGX_CR_TORNADO_PERF_SELECT0,         0x0000,  RGX_CR_TORNADO_PERF_COUNTER_0,        4,              1,                              21,                  4,  "RGX_CR_TORNADO_PERF",         rgxfw_hwperf_pow_st_direct },
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_TORNADO),
#endif

	/*RGX_CNTBLK_ID_JONES*/
#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
    {RGX_CNTBLK_ID_JONES,   0, /* direct */                 RGX_CR_JONES_PERF,          RGX_CR_JONES_PERF_SELECT0,           0x0000,  RGX_CR_JONES_PERF_COUNTER_0,          4,              1,                              21,                  3,  "RGX_CR_JONES_PERF",           rgxfw_hwperf_pow_st_jones },
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_JONES),
#endif

	/*RGX_CNTBLK_ID_BF RGX_CNTBLK_ID_BT RGX_CNTBLK_ID_RT RGX_CNTBLK_ID_SH*/
#if defined(RGX_FEATURE_RAY_TRACING)

    {RGX_CNTBLK_ID_BF,      0, /* direct */                 DPX_CR_BF_PERF,             DPX_CR_BF_PERF_SELECT0,              0x0000,  DPX_CR_BF_PERF_COUNTER_0,             4,              1,                              21,                  3,  "RGX_CR_BF_PERF",              rgxfw_hwperf_pow_st_gandalf },

    {RGX_CNTBLK_ID_BT,      0, /* direct */                 DPX_CR_BT_PERF,             DPX_CR_BT_PERF_SELECT0,              0x0000,  DPX_CR_BT_PERF_COUNTER_0,             4,              1,                              21,                  3,  "RGX_CR_BT_PERF",              rgxfw_hwperf_pow_st_gandalf },

    {RGX_CNTBLK_ID_RT,      0, /* direct */                 DPX_CR_RT_PERF,             DPX_CR_RT_PERF_SELECT0,              0x0000,  DPX_CR_RT_PERF_COUNTER_0,             4,              1,                              21,                  3,  "RGX_CR_RT_PERF",              rgxfw_hwperf_pow_st_gandalf },

    {RGX_CNTBLK_ID_SH,      0, /* direct */                 RGX_CR_SH_PERF,             RGX_CR_SH_PERF_SELECT0,              0x0000,  RGX_CR_SH_PERF_COUNTER_0,             4,              1,                              21,                  3,  "RGX_CR_SH_PERF",              rgxfw_hwperf_pow_st_gandalf },
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_BF),
	RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_BT),
	RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_RT),
	RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_SH),
#endif

	/*RGX_CNTBLK_ID_TPU_MCU0*/
#if defined(RGX_FEATURE_PERFBUS)
	{RGX_CNTBLK_ID_TPU_MCU0,
#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE) 
                            RGX_CR_TPU_PERF_INDIRECT,
#else
                            RGX_CR_TPU_MCU_L0_PERF_INDIRECT,
#endif
                                                            RGX_CR_TPU_MCU_L0_PERF,     RGX_CR_TPU_MCU_L0_PERF_SELECT0,
#if defined(HW_ERN_41805)
                                                                                                                            0x8000,
#else
                                                                                                                            0x0000,
#endif
                                                                                                                                      RGX_CR_TPU_MCU_L0_PERF_COUNTER_0,     4,              RGX_HWPERF_PHANTOM_INDIRECT_BY_DUST, 21,             3,  "RGX_CR_TPU_MCU_L0_PERF",      rgxfw_hwperf_pow_st_indirect },
#else
	RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_TPU_MCU0),
#endif

	/*RGX_CNTBLK_ID_USC0*/
#if defined(RGX_FEATURE_PERFBUS)
    {RGX_CNTBLK_ID_USC0,    RGX_CR_USC_PERF_INDIRECT,       RGX_CR_USC_PERF,            RGX_CR_USC_PERF_SELECT0,            0x0000,   RGX_CR_USC_PERF_COUNTER_0,            4,              RGX_HWPERF_PHANTOM_INDIRECT_BY_CLUSTER, 21,          3,  "RGX_CR_USC_PERF",             rgxfw_hwperf_pow_st_indirect },
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_USC0),
#endif

	/*RGX_CNTBLK_ID_TEXAS0*/
#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
    {RGX_CNTBLK_ID_TEXAS0,  RGX_CR_TEXAS3_PERF_INDIRECT,    RGX_CR_TEXAS_PERF,          RGX_CR_TEXAS_PERF_SELECT0,          0x0000,   RGX_CR_TEXAS_PERF_COUNTER_0,          6,              RGX_HWPERF_PHANTOM_INDIRECT_BY_DUST, 31,             3,  "RGX_CR_TEXAS_PERF",           rgxfw_hwperf_pow_st_indirect },
#elif defined(RGX_FEATURE_XT_TOP_INFRASTRUCTURE)
    {RGX_CNTBLK_ID_TEXAS0,  RGX_CR_TEXAS_PERF_INDIRECT,     RGX_CR_TEXAS_PERF,          RGX_CR_TEXAS_PERF_SELECT0,          0x0000,   RGX_CR_TEXAS_PERF_COUNTER_0,          6,              RGX_HWPERF_INDIRECT_BY_PHANTOM, 31,                  3,  "RGX_CR_TEXAS_PERF",           rgxfw_hwperf_pow_st_indirect },
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_TEXAS0),
#endif

	/*RGX_CNTBLK_ID_RASTER0*/
#if defined(RGX_FEATURE_XT_TOP_INFRASTRUCTURE)
    {RGX_CNTBLK_ID_RASTER0, RGX_CR_RASTERISATION_PERF_INDIRECT, RGX_CR_RASTERISATION_PERF, RGX_CR_RASTERISATION_PERF_SELECT0, 0x0000, RGX_CR_RASTERISATION_PERF_COUNTER_0,  4,              RGX_HWPERF_INDIRECT_BY_PHANTOM, 21,                  3,  "RGX_CR_RASTERISATION_PERF",   rgxfw_hwperf_pow_st_indirect },
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_RASTER0),
#endif

	/*RGX_CNTBLK_ID_BLACKPEARL0*/
#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
    {RGX_CNTBLK_ID_BLACKPEARL0, RGX_CR_BLACKPEARL_PERF_INDIRECT, RGX_CR_BLACKPEARL_PERF, RGX_CR_BLACKPEARL_PERF_SELECT0,    0x0000,   RGX_CR_BLACKPEARL_PERF_COUNTER_0,     6,              RGX_HWPERF_INDIRECT_BY_PHANTOM, 21,                  3,  "RGX_CR_BLACKPEARL_PERF",      rgxfw_hwperf_pow_st_indirect },
#else
	RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_BLACKPEARL0),
#endif

	/*RGX_CNTBLK_ID_PBE0*/
#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
    {RGX_CNTBLK_ID_PBE0,    RGX_CR_PBE_PERF_INDIRECT, RGX_CR_PBE_PERF,                  RGX_CR_PBE_PERF_SELECT0,            0x0000,   RGX_CR_PBE_PERF_COUNTER_0,            4,              RGX_HWPERF_PHANTOM_INDIRECT_BY_CLUSTER, 21,          3,  "RGX_CR_PBE_PERF",             rgxfw_hwperf_pow_st_indirect },
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_PBE0),
#endif

	/*RGX_CNTBLK_ID_BX_TU0*/
#if defined (RGX_FEATURE_RAY_TRACING)
    {RGX_CNTBLK_ID_BX_TU0, RGX_CR_BX_TU_PERF_INDIRECT,     DPX_CR_BX_TU_PERF,          DPX_CR_BX_TU_PERF_SELECT0,           0x0000,   DPX_CR_BX_TU_PERF_COUNTER_0,          4,              RGX_HWPERF_DOPPLER_BX_TU_BLKS,       21,                  3,  "RGX_CR_BX_TU_PERF",           rgxfw_hwperf_pow_st_gandalf },
#else
    RGXFW_HWPERF_CNTBLK_TYPE_UNSUPPORTED(RGX_CNTBLK_ID_BX_TU0),
#endif
};

IMG_INTERNAL IMG_UINT32
RGXGetHWPerfBlockConfig(const RGXFW_HWPERF_CNTBLK_TYPE_MODEL **ppsModel)
{
	*ppsModel = gasCntBlkTypeModel;
	return IMG_ARR_NUM_ELEMS(gasCntBlkTypeModel);
}

