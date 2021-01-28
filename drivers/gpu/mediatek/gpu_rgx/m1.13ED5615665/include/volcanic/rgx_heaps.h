/*************************************************************************/ /*!
@File
@Title          RGX heap definitions
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

#if !defined(RGX_HEAPS_H)
#define RGX_HEAPS_H

#include "km/rgxdefs_km.h"
#include "img_defs.h"
#include "log2.h"
#include "pvr_debug.h"

/* RGX Heap IDs, note: not all heaps are available to clients */
#define RGX_UNDEFINED_HEAP_ID					(~0LU)			/*!< RGX Undefined Heap ID */
#define RGX_GENERAL_SVM_HEAP_ID					0				/*!< RGX General SVM (shared virtual memory) Heap ID */
#define RGX_GENERAL_HEAP_ID						1				/*!< RGX General Heap ID */
#define RGX_GENERAL_NON4K_HEAP_ID				2				/*!< RGX General none-4K Heap ID */
#define RGX_PDSCODEDATA_HEAP_ID					3				/*!< RGX PDS Code/Data Heap ID */
#define RGX_USCCODE_HEAP_ID						4				/*!< RGX USC Code Heap ID */
#define RGX_FIRMWARE_MAIN_HEAP_ID				5				/*!< RGX Main Firmware Heap ID */
#define RGX_TQ3DPARAMETERS_HEAP_ID				6				/*!< RGX Firmware Heap ID */
#define RGX_SIGNALS_HEAP_ID						7				/*!< Compute Signals Heap ID */
#define RGX_COMPONENT_CTRL_HEAP_ID				8				/*!< DCE Component Ctrl Heap ID */
#define RGX_FBCDC_HEAP_ID						9				/*!< FBCDC State Table Heap ID */
#define RGX_PDS_INDIRECT_STATE_HEAP_ID			10				/*!< PDS Indirect State Table Heap ID */
#define RGX_TEXTURE_STATE_HEAP_ID				11				/*!< Texture State Heap ID */
#define RGX_TDM_TPU_YUV_COEFFS_HEAP_ID			12
#define RGX_VISIBILITY_TEST_HEAP_ID				13
#define RGX_FIRMWARE_CONFIG_HEAP_ID				14				/*!< RGX Main Firmware Heap ID */
#define RGX_GUEST_FIRMWARE_RAW_HEAP_ID			15				/*!< Additional OSIDs Firmware */
#define RGX_MAX_HEAP_ID	(RGX_GUEST_FIRMWARE_RAW_HEAP_ID + RGX_NUM_OS_SUPPORTED)	/*!< Max Valid Heap ID */

/*
  Following heaps from the above HEAP IDs can have virtual address space reserved at the start
  of the heap. Offsets within this reserved range are intended to be shared between RGX clients
  and FW. Naming convention for these macros: Just replace the 'ID' suffix by 'RESERVED_SIZE'
  in heap ID macros.
  Reserved VA space of a heap must always be multiple of RGX_HEAP_RESERVED_SIZE_GRANULARITY,
  this check is validated in the DDK. Note this is only reserving "Virtual Address" space and
  physical allocations (and mappings thereon) should only be done as much as required (to avoid
  wastage).
  Granularity has been chosen to support the max possible practically used OS page size.
*/
#define RGX_HEAP_RESERVED_SIZE_GRANULARITY        0x10000 /* 64KB is MAX anticipated OS page size */
#define RGX_GENERAL_HEAP_RESERVED_SIZE            1 * RGX_HEAP_RESERVED_SIZE_GRANULARITY
#define RGX_PDSCODEDATA_HEAP_RESERVED_SIZE        1 * RGX_HEAP_RESERVED_SIZE_GRANULARITY
#define RGX_USCCODE_HEAP_RESERVED_SIZE            1 * RGX_HEAP_RESERVED_SIZE_GRANULARITY
#define RGX_VK_CAPT_REPLAY_BUF_HEAP_RESERVED_SIZE (0)
#define RGX_SIGNALS_HEAP_RESERVED_SIZE            0
#define RGX_COMPONENT_CTRL_HEAP_RESERVED_SIZE     0
#define RGX_FBCDC_HEAP_RESERVED_SIZE              0
#define RGX_PDS_INDIRECT_STATE_HEAP_RESERVED_SIZE 0
#define RGX_TEXTURE_STATE_HEAP_RESERVED_SIZE      0
#define RGX_VISIBILITY_TEST_HEAP_RESERVED_SIZE    0

/*
  Identify heaps by their names
*/
#define RGX_GENERAL_SVM_HEAP_IDENT				"General SVM"					/*!< SVM (shared virtual memory) Heap Identifier */
#define RGX_GENERAL_HEAP_IDENT					"General"						/*!< RGX General Heap Identifier */
#define RGX_GENERAL_NON4K_HEAP_IDENT			"General NON-4K"				/*!< RGX General non-4K Heap Identifier */
#define RGX_PDSCODEDATA_HEAP_IDENT				"PDS Code and Data"				/*!< RGX PDS Code/Data Heap Identifier */
#define RGX_USCCODE_HEAP_IDENT					"USC Code"						/*!< RGX USC Code Heap Identifier */
#define RGX_TQ3DPARAMETERS_HEAP_IDENT			"TQ3DParameters"				/*!< RGX TQ 3D Parameters Heap Identifier */
#define RGX_SIGNALS_HEAP_IDENT					"Signals"						/*!< Compute Signals Heap Identifier */
#define RGX_COMPONENT_CTRL_HEAP_IDENT			"Component Control"				/*!< RGX DCE Component Control Heap Identifier */
#define RGX_FBCDC_HEAP_IDENT					"FBCDC"							/*!< RGX FBCDC State Table Heap Identifier */
#define RGX_PDS_INDIRECT_STATE_HEAP_IDENT		"PDS Indirect State"			/*!< PDS Indirect State Table Heap Identifier */
#define RGX_TEXTURE_STATE_HEAP_IDENT			"Texture State"					/*!< Texture State Heap Identifier */
#define RGX_TDM_TPU_YUV_COEFFS_HEAP_IDENT		"TDM TPU YUV Coeffs"
#define RGX_VISIBILITY_TEST_HEAP_IDENT			"Visibility Test"
#define RGX_FIRMWARE_MAIN_HEAP_IDENT			"Firmware Main"					/*!< RGX Main Firmware Heap identifier */
#define RGX_FIRMWARE_CONFIG_HEAP_IDENT			"Firmware Config"				/*!< RGX Config firmware Heap identifier */
#define RGX_FIRMWARE_GUEST_RAW_HEAP_IDENT		"Firmware Raw Guest %d"			/*!< Combined Heap from which a Guest OS'
																					 Main and Config heaps are allocated */
#define RGX_VK_CAPT_REPLAY_BUF_HEAP_IDENT		"Vulkan capture replay buffer"	/*!< RGX vulkan capture replay buffer Heap Identifier */

/*
 *  Supported log2 page size values for RGX_GENERAL_NON_4K_HEAP_ID
 */
#define RGX_HEAP_4KB_PAGE_SHIFT					(12U)
#define RGX_HEAP_16KB_PAGE_SHIFT				(14U)
#define RGX_HEAP_64KB_PAGE_SHIFT				(16U)
#define RGX_HEAP_256KB_PAGE_SHIFT				(18U)
#define RGX_HEAP_1MB_PAGE_SHIFT					(20U)
#define RGX_HEAP_2MB_PAGE_SHIFT					(21U)

/* Takes a log2 page size parameter and calculates a suitable page size
 * for the RGX heaps. Returns 0 if parameter is wrong.*/
static INLINE IMG_UINT32 RGXHeapDerivePageSize(IMG_UINT32 uiLog2PageSize)
{
	IMG_BOOL bFound = IMG_FALSE;

	/* OS page shift must be at least RGX_HEAP_4KB_PAGE_SHIFT,
	 * max RGX_HEAP_2MB_PAGE_SHIFT, non-zero and a power of two*/
	if (uiLog2PageSize == 0U ||
	    (uiLog2PageSize < RGX_HEAP_4KB_PAGE_SHIFT) ||
	    (uiLog2PageSize > RGX_HEAP_2MB_PAGE_SHIFT))
	{
		PVR_DPF((PVR_DBG_ERROR,
				"%s: Provided incompatible log2 page size %u",
				__func__,
				uiLog2PageSize));
		PVR_ASSERT(0);
		return 0;
	}

	do
	{
		switch (uiLog2PageSize)
		{
			case RGX_HEAP_4KB_PAGE_SHIFT:
			case RGX_HEAP_16KB_PAGE_SHIFT:
			case RGX_HEAP_64KB_PAGE_SHIFT:
			case RGX_HEAP_256KB_PAGE_SHIFT:
			case RGX_HEAP_1MB_PAGE_SHIFT:
			case RGX_HEAP_2MB_PAGE_SHIFT:
				/* All good, RGX page size equals given page size
				 * => use it as default for heaps */
				bFound = IMG_TRUE;
				break;
			default:
				/* We have to fall back to a smaller device
				 * page size than given page size because there
				 * is no exact match for any supported size. */
				uiLog2PageSize -= 1U;
				break;
		}
	} while (!bFound);

	return uiLog2PageSize;
}


#endif /* RGX_HEAPS_H */
