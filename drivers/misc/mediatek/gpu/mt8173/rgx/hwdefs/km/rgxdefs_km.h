/*************************************************************************/ /*!
@Title          Rogue hw definitions (kernel mode)
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

#ifndef _RGXDEFS_KM_H_
#define _RGXDEFS_KM_H_

#include RGX_BVNC_CORE_KM_HEADER
#include RGX_BNC_CONFIG_KM_HEADER

#define __IMG_EXPLICIT_INCLUDE_HWDEFS
#include "rgx_cr_defs_km.h"
#undef __IMG_EXPLICIT_INCLUDE_HWDEFS

/******************************************************************************
 * Check for valid B.X.N.C
 *****************************************************************************/
#if !defined(RGX_BVNC_KM_B) || !defined(RGX_BVNC_KM_V) || !defined(RGX_BVNC_KM_N) || !defined(RGX_BVNC_KM_C)
#error "Need to specify BVNC (RGX_BVNC_KM_B, RGX_BVNC_KM_V, RGX_BVNC_KM_N and RGX_BVNC_C)"
#endif

/* Check core/config compatibility */
#if (RGX_BVNC_KM_B != RGX_BNC_KM_B) || (RGX_BVNC_KM_N != RGX_BNC_KM_N) || (RGX_BVNC_KM_C != RGX_BNC_KM_C) 
#error "BVNC headers are mismatching (KM core/config)"
#endif

/******************************************************************************
 * RGX Version name
 *****************************************************************************/
#define _RGX_BVNC_ST2(S)	#S
#define _RGX_BVNC_ST(S)		_RGX_BVNC_ST2(S)
#define RGX_BVNC_KM			_RGX_BVNC_ST(RGX_BVNC_KM_B) "." _RGX_BVNC_ST(RGX_BVNC_KM_V) "." _RGX_BVNC_ST(RGX_BVNC_KM_N) "." _RGX_BVNC_ST(RGX_BVNC_KM_C)
#define RGX_BVNC_KM_V_ST	_RGX_BVNC_ST(RGX_BVNC_KM_V)

/******************************************************************************
 * RGX Defines
 *****************************************************************************/

#if defined(RGX_FEATURE_META)
/* META cores (required for the RGX_FEATURE_META) */
#define MTP218   (1)
#define MTP219   (2)
#define LTP218   (3)
#define LTP217   (4)

/* META Core memory feature depending on META variants */
#define RGX_META_COREMEM_32K      (32*1024)
#define RGX_META_COREMEM_48K      (48*1024)
#define RGX_META_COREMEM_64K      (64*1024)
#define RGX_META_COREMEM_256K     (256*1024)

#if defined(SUPPORT_TRUSTED_DEVICE)
#undef SUPPORT_META_COREMEM
#undef RGX_FEATURE_META_COREMEM_SIZE
#undef RGX_FEATURE_META_DMA
#define RGX_FEATURE_META_COREMEM_SIZE (0)
#define RGX_META_COREMEM_SIZE         (0)
#else
#define RGX_META_COREMEM_SIZE         (RGX_FEATURE_META_COREMEM_SIZE*1024)
#endif

#if (RGX_FEATURE_META_COREMEM_SIZE != 0)
#define RGX_META_COREMEM          (1)
#define RGX_META_COREMEM_CODE     (1)
#if !defined(FIX_HW_BRN_50767)
#define RGX_META_COREMEM_DATA     (1)
#endif
#endif

#endif  /*RGX_FEATURE_META*/

/* ISP requires valid state on all three pipes regardless of the number of
 * active pipes/tiles in flight.
 */
#define RGX_MAX_NUM_PIPES	3

#define ROGUE_CACHE_LINE_SIZE				((RGX_FEATURE_SLC_CACHE_LINE_SIZE_BITS)/8)

#define MAX_HW_TA3DCONTEXTS	2


/* useful extra defines for clock ctrl*/
#define RGX_CR_CLK_CTRL_ALL_ON   (IMG_UINT64_C(0x5555555555555555)&RGX_CR_CLK_CTRL_MASKFULL)
#define RGX_CR_CLK_CTRL_ALL_AUTO (IMG_UINT64_C(0xaaaaaaaaaaaaaaaa)&RGX_CR_CLK_CTRL_MASKFULL)

#define RGX_MAX_DUST		MAX(1, RGX_FEATURE_NUM_CLUSTERS/2)

#define RGX_CR_SOFT_RESET_DUST_n_CORE_EN	(RGX_CR_SOFT_RESET_DUST_A_CORE_EN | \
											 RGX_CR_SOFT_RESET_DUST_B_CORE_EN | \
											 RGX_CR_SOFT_RESET_DUST_C_CORE_EN | \
											 RGX_CR_SOFT_RESET_DUST_D_CORE_EN | \
											 RGX_CR_SOFT_RESET_DUST_E_CORE_EN | \
											 RGX_CR_SOFT_RESET_DUST_F_CORE_EN | \
											 RGX_CR_SOFT_RESET_DUST_G_CORE_EN | \
											 RGX_CR_SOFT_RESET_DUST_H_CORE_EN)

/* SOFT_RESET Rascal and DUSTs bits */
#define RGX_CR_SOFT_RESET_RASCALDUSTS_EN	(RGX_CR_SOFT_RESET_RASCAL_CORE_EN | \
											 RGX_CR_SOFT_RESET_DUST_n_CORE_EN)



#if defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE)
/* SOFT_RESET steps as defined in the TRM */
#define RGX_S7_SOFT_RESET_DUSTS (RGX_CR_SOFT_RESET_DUST_n_CORE_EN)

#define RGX_S7_SOFT_RESET_JONES (RGX_CR_SOFT_RESET_PM_EN  | \
                                 RGX_CR_SOFT_RESET_VDM_EN | \
								 RGX_CR_SOFT_RESET_ISP_EN)

#define RGX_S7_SOFT_RESET_JONES_ALL (RGX_S7_SOFT_RESET_JONES  | \
									 RGX_CR_SOFT_RESET_BIF_EN | \
                                     RGX_CR_SOFT_RESET_SLC_EN | \
								     RGX_CR_SOFT_RESET_GARTEN_EN)

#define RGX_S7_SOFT_RESET2 (RGX_CR_SOFT_RESET2_BLACKPEARL_EN | \
                            RGX_CR_SOFT_RESET2_PIXEL_EN | \
							RGX_CR_SOFT_RESET2_CDM_EN | \
							RGX_CR_SOFT_RESET2_VERTEX_EN)
#endif


#define RGX_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT		(12)
#define RGX_BIF_PM_PHYSICAL_PAGE_SIZE			(1 << RGX_BIF_PM_PHYSICAL_PAGE_ALIGNSHIFT)

#define RGX_BIF_PM_VIRTUAL_PAGE_ALIGNSHIFT		(14)
#define RGX_BIF_PM_VIRTUAL_PAGE_SIZE			(1 << RGX_BIF_PM_VIRTUAL_PAGE_ALIGNSHIFT)

/* To get the number of required Dusts, divide the number of clusters by 2 and round up */
#define RGX_REQ_NUM_DUSTS(CLUSTERS)    ((CLUSTERS + 1) / 2)

/* To get the number of required Bernado/Phantom, divide the number of clusters by 4 and round up */
#define RGX_REQ_NUM_PHANTOMS(CLUSTERS) ((CLUSTERS + 3) / 4)
#define RGX_REQ_NUM_BERNADOS(CLUSTERS) ((CLUSTERS + 3) / 4)
#define RGX_REQ_NUM_BLACKPEARLS(CLUSTERS) ((CLUSTERS + 3) / 4)

#if defined(RGX_FEATURE_CLUSTER_GROUPING)
#define RGX_NUM_PHANTOMS (RGX_REQ_NUM_PHANTOMS(RGX_FEATURE_NUM_CLUSTERS))
#else
#define RGX_NUM_PHANTOMS (1)
#endif

/* RGX_FEATURE_CDM_CONTROL_STREAM_FORMAT is not defined for format 1 cores (so define it now). */
#if !defined(RGX_FEATURE_CDM_CONTROL_STREAM_FORMAT)
#define RGX_FEATURE_CDM_CONTROL_STREAM_FORMAT (1)
#endif

/* META second thread feature depending on META variants and available CoreMem*/
#if defined(RGX_FEATURE_META) && (RGX_FEATURE_META == MTP218 || RGX_FEATURE_META == MTP219) && defined(RGX_FEATURE_S7_TOP_INFRASTRUCTURE) && (RGX_FEATURE_META_COREMEM_SIZE == 256)
#define RGXFW_META_SUPPORT_2ND_THREAD
#endif

#if RGX_FEATURE_VIRTUAL_ADDRESS_SPACE_BITS == 40
/* 
   Start at 903GiB. Size of 32MB per OSID (see rgxheapconfig.h) 
   NOTE:
		The firmware heap base and size is defined here to
		simplify #include dependencies, see rgxheapconfig.h
		for the full RGX virtual address space layout.
*/
#define RGX_FIRMWARE_HEAP_BASE			IMG_UINT64_C(0xE1C0000000)
#define RGX_FIRMWARE_HEAP_SIZE			(1<<RGX_FW_HEAP_SHIFT)
#define RGX_FIRMWARE_HEAP_SHIFT			RGX_FW_HEAP_SHIFT
#endif

/* Default number of OSIDs is 1 unless GPU Virtualization is supported and enabled */
#if defined(SUPPORT_PVRSRV_GPUVIRT) && !defined(PVRSRV_GPUVIRT_GUESTDRV) && (PVRSRV_GPUVIRT_NUM_OSID +1> 1)
#define RGXFW_NUM_OS PVRSRV_GPUVIRT_NUM_OSID
#else
#define RGXFW_NUM_OS 1
#endif

/******************************************************************************
 * WA HWBRNs
 *****************************************************************************/
#if defined(FIX_HW_BRN_36492)

#undef RGX_CR_SOFT_RESET_SLC_EN
#undef RGX_CR_SOFT_RESET_SLC_CLRMSK
#undef RGX_CR_SOFT_RESET_SLC_SHIFT

/* Remove the SOFT_RESET_SLC_EN bit from SOFT_RESET_MASKFULL */
#undef RGX_CR_SOFT_RESET_MASKFULL 
#define RGX_CR_SOFT_RESET_MASKFULL IMG_UINT64_C(0x000001FFF7FFFC1D)

#endif /* FIX_HW_BRN_36492 */


#if defined(RGX_CR_JONES_IDLE_MASKFULL) && !defined(RGX_FEATURE_FASTRENDER_DM)
/* Workaround for HW BRN 57289 */
#if (RGX_CR_JONES_IDLE_MASKFULL != 0x0000000000007FFF)
#error This WA must be updated if RGX_CR_JONES_IDLE is expanded!!!
#endif
#undef RGX_CR_JONES_IDLE_MASKFULL
#undef RGX_CR_JONES_IDLE_TDM_SHIFT
#undef RGX_CR_JONES_IDLE_TDM_CLRMSK
#undef RGX_CR_JONES_IDLE_TDM_EN
#define RGX_CR_JONES_IDLE_MASKFULL                        (IMG_UINT64_C(0x0000000000003FFF))
#endif


#define DPX_MAX_RAY_CONTEXTS 4 /* FIXME should this be in dpx file? */
#define DPX_MAX_FBA_AP 16
#define DPX_MAX_FBA_FILTER_WIDTH 24

#if !defined(RGX_FEATURE_SLC_SIZE_IN_BYTES)
#if defined(RGX_FEATURE_SLC_SIZE_IN_KILOBYTES)
#define RGX_FEATURE_SLC_SIZE_IN_BYTES (RGX_FEATURE_SLC_SIZE_IN_KILOBYTES * 1024)
#else
#define RGX_FEATURE_SLC_SIZE_IN_BYTES (0)
#endif
#endif

#endif /* _RGXDEFS_KM_H_ */
