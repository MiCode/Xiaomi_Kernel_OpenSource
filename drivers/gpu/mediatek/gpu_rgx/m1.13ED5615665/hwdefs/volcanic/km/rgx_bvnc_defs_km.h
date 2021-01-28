/*************************************************************************/ /*!
@Title          Hardware definition file rgx_bvnc_defs_km.h
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

/******************************************************************************
 *                 Auto generated file by rgxbvnc_tablegen.py                 *
 *                  This file should not be edited manually                   *
 *****************************************************************************/

#ifndef RGX_BVNC_DEFS_KM_H
#define RGX_BVNC_DEFS_KM_H

#include "img_types.h"
#include "img_defs.h"

#if defined(RGX_BVNC_DEFS_UM_H)
#error "This file should not be included in conjunction with rgx_bvnc_defs_um.h"
#endif

#define BVNC_FIELD_WIDTH (16U)


/******************************************************************************
 * Mask and bit-position macros for features without values
 *****************************************************************************/

#define	RGX_FEATURE_ALBIORIX_TOP_INFRASTRUCTURE_POS                 	(0U)
#define	RGX_FEATURE_ALBIORIX_TOP_INFRASTRUCTURE_BIT_MASK            	(IMG_UINT64_C(0x0000000000000001))

#define	RGX_FEATURE_AXI_ACE_POS                                     	(1U)
#define	RGX_FEATURE_AXI_ACE_BIT_MASK                                	(IMG_UINT64_C(0x0000000000000002))

#define	RGX_FEATURE_GPU_CPU_COHERENCY_POS                           	(2U)
#define	RGX_FEATURE_GPU_CPU_COHERENCY_BIT_MASK                      	(IMG_UINT64_C(0x0000000000000004))

#define	RGX_FEATURE_GPU_VIRTUALISATION_POS                          	(3U)
#define	RGX_FEATURE_GPU_VIRTUALISATION_BIT_MASK                     	(IMG_UINT64_C(0x0000000000000008))

#define	RGX_FEATURE_HYPERVISOR_MMU_POS                              	(4U)
#define	RGX_FEATURE_HYPERVISOR_MMU_BIT_MASK                         	(IMG_UINT64_C(0x0000000000000010))

#define	RGX_FEATURE_META_DMA_POS                                    	(5U)
#define	RGX_FEATURE_META_DMA_BIT_MASK                               	(IMG_UINT64_C(0x0000000000000020))

#define	RGX_FEATURE_META_REGISTER_UNPACKED_ACCESSES_POS             	(6U)
#define	RGX_FEATURE_META_REGISTER_UNPACKED_ACCESSES_BIT_MASK        	(IMG_UINT64_C(0x0000000000000040))

#define	RGX_FEATURE_PBE_CHECKSUM_2D_POS                             	(7U)
#define	RGX_FEATURE_PBE_CHECKSUM_2D_BIT_MASK                        	(IMG_UINT64_C(0x0000000000000080))

#define	RGX_FEATURE_PERFBUS_POS                                     	(8U)
#define	RGX_FEATURE_PERFBUS_BIT_MASK                                	(IMG_UINT64_C(0x0000000000000100))

#define	RGX_FEATURE_PM_BYTE_ALIGNED_BASE_ADDRESSES_POS              	(9U)
#define	RGX_FEATURE_PM_BYTE_ALIGNED_BASE_ADDRESSES_BIT_MASK         	(IMG_UINT64_C(0x0000000000000200))

#define	RGX_FEATURE_PM_MMUSTACK_POS                                 	(10U)
#define	RGX_FEATURE_PM_MMUSTACK_BIT_MASK                            	(IMG_UINT64_C(0x0000000000000400))

#define	RGX_FEATURE_PM_MMU_VFP_POS                                  	(11U)
#define	RGX_FEATURE_PM_MMU_VFP_BIT_MASK                             	(IMG_UINT64_C(0x0000000000000800))

#define	RGX_FEATURE_SIGNAL_SNOOPING_POS                             	(12U)
#define	RGX_FEATURE_SIGNAL_SNOOPING_BIT_MASK                        	(IMG_UINT64_C(0x0000000000001000))

#define	RGX_FEATURE_SLC_FAULT_ACCESS_ADDR_PHYS_POS                  	(13U)
#define	RGX_FEATURE_SLC_FAULT_ACCESS_ADDR_PHYS_BIT_MASK             	(IMG_UINT64_C(0x0000000000002000))

#define	RGX_FEATURE_SLC_VIVT_POS                                    	(14U)
#define	RGX_FEATURE_SLC_VIVT_BIT_MASK                               	(IMG_UINT64_C(0x0000000000004000))

#define	RGX_FEATURE_SYS_BUS_SECURE_RESET_POS                        	(15U)
#define	RGX_FEATURE_SYS_BUS_SECURE_RESET_BIT_MASK                   	(IMG_UINT64_C(0x0000000000008000))

#define	RGX_FEATURE_TDM_PDS_CHECKSUM_POS                            	(16U)
#define	RGX_FEATURE_TDM_PDS_CHECKSUM_BIT_MASK                       	(IMG_UINT64_C(0x0000000000010000))

#define	RGX_FEATURE_TESSELLATION_POS                                	(17U)
#define	RGX_FEATURE_TESSELLATION_BIT_MASK                           	(IMG_UINT64_C(0x0000000000020000))

#define	RGX_FEATURE_ZLS_CHECKSUM_POS                                	(18U)
#define	RGX_FEATURE_ZLS_CHECKSUM_BIT_MASK                           	(IMG_UINT64_C(0x0000000000040000))


/******************************************************************************
 * Features with values indexes
 *****************************************************************************/

typedef enum _RGX_FEATURE_WITH_VALUE_INDEX_ {
	RGX_FEATURE_CONTEXT_SWITCH_3D_LEVEL_IDX,
	RGX_FEATURE_ECC_RAMS_IDX,
	RGX_FEATURE_FBCDC_IDX,
	RGX_FEATURE_FBCDC_ARCHITECTURE_IDX,
	RGX_FEATURE_MAX_TPU_PER_SPU_IDX,
	RGX_FEATURE_META_IDX,
	RGX_FEATURE_META_COREMEM_BANKS_IDX,
	RGX_FEATURE_META_COREMEM_SIZE_IDX,
	RGX_FEATURE_META_DMA_CHANNEL_COUNT_IDX,
	RGX_FEATURE_MMU_VERSION_IDX,
	RGX_FEATURE_NUM_CLUSTERS_IDX,
	RGX_FEATURE_NUM_ISP_IPP_PIPES_IDX,
	RGX_FEATURE_NUM_ISP_PER_SPU_IDX,
	RGX_FEATURE_NUM_MEMBUS_IDX,
	RGX_FEATURE_NUM_OSIDS_IDX,
	RGX_FEATURE_NUM_SPU_IDX,
	RGX_FEATURE_PBE_PER_SPU_IDX,
	RGX_FEATURE_PHYS_BUS_WIDTH_IDX,
	RGX_FEATURE_SCALABLE_TE_ARCH_IDX,
	RGX_FEATURE_SCALABLE_VCE_IDX,
	RGX_FEATURE_SLC_BANKS_IDX,
	RGX_FEATURE_SLC_CACHE_LINE_SIZE_BITS_IDX,
	RGX_FEATURE_SLC_SIZE_IN_KILOBYTES_IDX,
	RGX_FEATURE_TILE_SIZE_X_IDX,
	RGX_FEATURE_TILE_SIZE_Y_IDX,
	RGX_FEATURE_VIRTUAL_ADDRESS_SPACE_BITS_IDX,
	RGX_FEATURE_WITH_VALUES_MAX_IDX,
} RGX_FEATURE_WITH_VALUE_INDEX;


/******************************************************************************
 * Mask and bit-position macros for ERNs and BRNs
 *****************************************************************************/

#define	HW_ERN_66574_POS                                            	(0U)
#define	HW_ERN_66574_BIT_MASK                                       	(IMG_UINT64_C(0x0000000000000001))

#define	FIX_HW_BRN_68777_POS                                        	(1U)
#define	FIX_HW_BRN_68777_BIT_MASK                                   	(IMG_UINT64_C(0x0000000000000002))

/* Macro used for padding the unavailable values for features with values */
#define RGX_FEATURE_VALUE_INVALID	(0xFFFFFFFEU)

/* Macro used for marking a feature with value as disabled for a specific bvnc */
#define RGX_FEATURE_VALUE_DISABLED	(0xFFFFFFFFU)

#endif /* RGX_BVNC_DEFS_KM_H */
