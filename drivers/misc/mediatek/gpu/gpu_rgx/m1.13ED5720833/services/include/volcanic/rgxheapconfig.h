/*************************************************************************/ /*!
@File
@Title          device configuration
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Memory heaps device specific configuration
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

#ifndef RGXHEAPCONFIG_H
#define RGXHEAPCONFIG_H

#include "rgxdefs_km.h"

/*
	RGX Device Virtual Address Space Definitions:

	Notes:
	Base addresses have to be a multiple of 4MiB

	RGX_PDSCODEDATA_HEAP_BASE and RGX_USCCODE_HEAP_BASE will be programmed, on a
	global basis, into RGX_CR_PDS_EXEC_BASE and RGX_CR_USC_CODE_BASE_*
	respectively. Therefore if clients use multiple configs they must still be
	consistent with	their definitions for these heaps.

	Shared virtual memory (GENERAL_SVM) support requires half of the address
	space be reserved for SVM allocations unless BRN fixes are required in
	which case the SVM heap is disabled.

	Variable page-size heap (GENERAL_NON4K) support splits available fixed
	4K page-size heap (GENERAL) address space in half. The actual page size
	defaults to 16K; AppHint PVRSRV_APPHINT_GENERALNON4KHEAPPAGESIZE
	can be used to forced it to these values: 4K,64K,256K,1M,2M.
*/

	/* GENERAL_SVM_HEAP - Start at 4MiB, Size of 512GiB less 4MiB */
	/* 0x0000400000 - 0x7FFFC00000 */
	#define RGX_GENERAL_SVM_HEAP_BASE			IMG_UINT64_C(0x0000400000)
	#define RGX_GENERAL_SVM_HEAP_SIZE			IMG_UINT64_C(0x7FFFC00000)

	/* GENERAL_HEAP - Start at 512GiB, Size of 64 GiB (Available 128GB) */
	/* 0x8000000000 - 0x8FFFFFFFFF */
	#define RGX_GENERAL_HEAP_BASE				IMG_UINT64_C(0x8000000000)
	#define RGX_GENERAL_HEAP_SIZE				IMG_UINT64_C(0x1000000000)

	#define RGX_GENERAL_NON4K_HEAP_BASE			(RGX_GENERAL_HEAP_BASE+RGX_GENERAL_HEAP_SIZE)
	#define RGX_GENERAL_NON4K_HEAP_SIZE			RGX_GENERAL_HEAP_SIZE

	/* Start at 640GiB. Size of 1 GiB */
	#define RGX_VK_CAPT_REPLAY_BUF_HEAP_BASE	(RGX_GENERAL_NON4K_HEAP_SIZE + RGX_GENERAL_NON4K_HEAP_SIZE)
	#define RGX_VK_CAPT_REPLAY_BUF_HEAP_SIZE	IMG_UINT64_C(0x0040000000)

	/* BIF_TILING_HEAP - 660GiB, Size of 32GiB */
	/* 0xA500000000 - 0xA6FFFFFFFF */
	#define RGX_BIF_TILING_NUM_HEAPS            4
	#define RGX_BIF_TILING_HEAP_SIZE            IMG_UINT64_C(0x0200000000)
	#define RGX_BIF_TILING_HEAP_1_BASE          IMG_UINT64_C(0xA500000000)
	#define RGX_BIF_TILING_HEAP_2_BASE          (RGX_BIF_TILING_HEAP_1_BASE + RGX_BIF_TILING_HEAP_SIZE)
	#define RGX_BIF_TILING_HEAP_3_BASE          (RGX_BIF_TILING_HEAP_2_BASE + RGX_BIF_TILING_HEAP_SIZE)
	#define RGX_BIF_TILING_HEAP_4_BASE          (RGX_BIF_TILING_HEAP_3_BASE + RGX_BIF_TILING_HEAP_SIZE)

	/* PDSCODEDATA_HEAP - Start at 700GiB, Size of 4 GiB */
	/* 0xAF00000000 - 0xAFFFFFFFFF */
	#define RGX_PDSCODEDATA_HEAP_BASE			IMG_UINT64_C(0xAF00000000)
	#define RGX_PDSCODEDATA_HEAP_SIZE			IMG_UINT64_C(0x0100000000)

	/* USCCODE_HEAP - Start at 708GiB, Size of 4GiB */
	/* 0xB100000000 - 0xB1FFFFFFFF */
	#define RGX_USCCODE_HEAP_BASE				IMG_UINT64_C(0xB100000000)
	#define RGX_USCCODE_HEAP_SIZE				IMG_UINT64_C(0x0100000000)

	/* Start at 903GiB. Firmware heaps defined in rgxdefs_km.h
		RGX_FIRMWARE_RAW_HEAP_BASE
		RGX_FIRMWARE_HOST_MAIN_HEAP_BASE
		RGX_FIRMWARE_GUEST_MAIN_HEAP_BASE
		RGX_FIRMWARE_MAIN_HEAP_SIZE
		RGX_FIRMWARE_CONFIG_HEAP_SIZE
		RGX_FIRMWARE_RAW_HEAP_SIZE */

	/* TQ3DPARAMETERS_HEAP - Start at 912GiB, Size 16GiB */
	/* 0xE400000000 - 0xE7FFFFFFFF (16GiB aligned to match RGX_CR_ISP_PIXEL_BASE) */
	#define RGX_TQ3DPARAMETERS_HEAP_BASE		IMG_UINT64_C(0xE400000000)
	#define RGX_TQ3DPARAMETERS_HEAP_SIZE		IMG_UINT64_C(0x0400000000)

	/* CDM Signals heap (31 signals less one reserved for Services). Start at 936GiB, 960bytes rounded up to 4K */
	#define RGX_SIGNALS_HEAP_BASE				IMG_UINT64_C(0xEA00000000)
	#define RGX_SIGNALS_HEAP_SIZE				IMG_UINT64_C(0x0000001000)

	/* COMPONENT_CTRL_HEAP - Start at 940GiB, Size 4GiB */
	/* 0xEB00000000 - 0xEB003FFFFF */
	#define RGX_COMPONENT_CTRL_HEAP_BASE		IMG_UINT64_C(0xEB00000000)
	#define RGX_COMPONENT_CTRL_HEAP_SIZE		IMG_UINT64_C(0x0100000000)

	/* FBCDC_HEAP - Start at 944GiB, Size 2MiB */
	/* 0xEC00000000 - 0xEC001FFFFF */
	#define RGX_FBCDC_HEAP_BASE					IMG_UINT64_C(0xEC00000000)
	#define RGX_FBCDC_HEAP_SIZE					IMG_UINT64_C(0x0000200000)

	/* FBCDC_LARGE_HEAP - Start at 945GiB, Size 2MiB */
	/* 0xEC40000000 - 0xEC401FFFFF */
	#define RGX_FBCDC_LARGE_HEAP_BASE			IMG_UINT64_C(0xEC40000000)
	#define RGX_FBCDC_LARGE_HEAP_SIZE			IMG_UINT64_C(0x0000200000)

	/* PDS_INDIRECT_STATE_HEAP - Start at 948GiB, Size 16MiB */
	/* 0xED00000000 - 0xED00FFFFFF */
	#define RGX_PDS_INDIRECT_STATE_HEAP_BASE	IMG_UINT64_C(0xED00000000)
	#define RGX_PDS_INDIRECT_STATE_HEAP_SIZE	IMG_UINT64_C(0x0001000000)

	/* RGX_TDM_TPU_YUV_COEFFS_HEAP - Start at 956GiB, Size 256KiB */
	/* 0xEE00080000 - 0xEE0003FFFF - (TDM TPU YUV coeffs - can fit 1 page) */
	#define RGX_TDM_TPU_YUV_COEFFS_HEAP_BASE	IMG_UINT64_C(0xEE00080000)
	#define RGX_TDM_TPU_YUV_COEFFS_HEAP_SIZE	IMG_UINT64_C(0x0000040000)

	/* TEXTURE_STATE_HEAP - Start at 960GiB, Size 4GiB */
	/* 0xF000000000 - 0xF0FFFFFFFF (36-bit aligned) */
	#define RGX_TEXTURE_STATE_HEAP_BASE			IMG_UINT64_C(0xF000000000)
	#define RGX_TEXTURE_STATE_HEAP_SIZE			IMG_UINT64_C(0x0100000000)

	/* VISIBILITY_TEST_HEAP - Start at 970GiB, Size 2MiB */
	#define RGX_VISIBILITY_TEST_HEAP_BASE		IMG_UINT64_C(0xF280000000)
	#define RGX_VISIBILITY_TEST_HEAP_SIZE		IMG_UINT64_C(0x0000200000)

	/* Heaps which are barred from using the reserved-region feature (intended for clients
	   of Services), but need the macro definitions are buried here */
	#define RGX_GENERAL_SVM_HEAP_RESERVED_SIZE        0  /* SVM heap is exclusively managed by USER or KERNEL */
	#define RGX_GENERAL_NON4K_HEAP_RESERVED_SIZE      0  /* Non-4K can have page sizes up to 2MB, which is currently
	                                                        not supported in reserved-heap implementation */
	/* ... and heaps which are not used outside of Services */
	#define RGX_TQ3DPARAMETERS_HEAP_RESERVED_SIZE     0
	#define RGX_TDM_TPU_YUV_COEFFS_HEAP_RESERVED_SIZE 0
	/* signal we've identified the core by the build */
	#define RGX_CORE_IDENTIFIED

#endif /* RGXHEAPCONFIG_H */

/******************************************************************************
 End of file (rgxheapconfig.h)
******************************************************************************/
