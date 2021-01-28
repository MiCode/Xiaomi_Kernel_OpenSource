/*************************************************************************/ /*!
@File           rgx_riscv.h
@Title
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Platform       RGX
@Description    RGX RISCV definitions, kernel/user space
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

#if !defined(RGX_RISCV_H)
#define RGX_RISCV_H

#include "km/rgxdefs_km.h"


/* Utility defines to convert regions to virtual addresses and remaps */
#define RGXRISCVFW_GET_REGION_BASE(r)                  IMG_UINT32_C((r) << 28)
#define RGXRISCVFW_GET_REGION(a)                       IMG_UINT32_C((a) >> 28)
#define RGXRISCVFW_MAX_REGION_SIZE                     IMG_UINT32_C(1 << 28)
#define RGXRISCVFW_GET_REMAP(r)                        (TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG + ((r) * 8U))

/* RISCV remap output is aligned to 4K */
#define RGXRISCVFW_REMAP_CONFIG_DEVVADDR_ALIGN         (0x1000U)

/*
 * FW bootloader defines
 */
#define RGXRISCVFW_BOOTLDR_CODE_REGION                 IMG_UINT32_C(0xC)
#define RGXRISCVFW_BOOTLDR_DATA_REGION                 IMG_UINT32_C(0x5)
#define RGXRISCVFW_BOOTLDR_CODE_BASE                   (RGXRISCVFW_GET_REGION_BASE(RGXRISCVFW_BOOTLDR_CODE_REGION))
#define RGXRISCVFW_BOOTLDR_DATA_BASE                   (RGXRISCVFW_GET_REGION_BASE(RGXRISCVFW_BOOTLDR_DATA_REGION))
#define RGXRISCVFW_BOOTLDR_CODE_REMAP                  (RGXRISCVFW_GET_REMAP(RGXRISCVFW_BOOTLDR_CODE_REGION))
#define RGXRISCVFW_BOOTLDR_DATA_REMAP                  (RGXRISCVFW_GET_REMAP(RGXRISCVFW_BOOTLDR_DATA_REGION))

/* Bootloader data offset in dwords from the beginning of the FW data allocation */
#define RGXRISCVFW_BOOTLDR_CONF_OFFSET                 (0x0)


/*
 * Host-FW shared data defines
 */
#define RGXRISCVFW_SHARED_CACHED_DATA_REGION           (0x6U)
#define RGXRISCVFW_SHARED_UNCACHED_DATA_REGION         (0xDU)
#define RGXRISCVFW_SHARED_CACHED_DATA_BASE             (RGXRISCVFW_GET_REGION_BASE(RGXRISCVFW_SHARED_CACHED_DATA_REGION))
#define RGXRISCVFW_SHARED_UNCACHED_DATA_BASE           (RGXRISCVFW_GET_REGION_BASE(RGXRISCVFW_SHARED_UNCACHED_DATA_REGION))
#define RGXRISCVFW_SHARED_CACHED_DATA_REMAP            (RGXRISCVFW_GET_REMAP(RGXRISCVFW_SHARED_CACHED_DATA_REGION))
#define RGXRISCVFW_SHARED_UNCACHED_DATA_REMAP          (RGXRISCVFW_GET_REMAP(RGXRISCVFW_SHARED_UNCACHED_DATA_REGION))


/* The things that follow are excluded when compiling assembly sources */
#if !defined(RGXRISCVFW_ASSEMBLY_CODE)
#include "img_types.h"

#define RGXFW_PROCESSOR_RISCV     "RISCV"
#define RGXRISCVFW_CORE_ID_VALUE  (0x00450B01U)

typedef struct
{
	IMG_UINT64 ui64CorememCodeDevVAddr;
	IMG_UINT64 ui64CorememDataDevVAddr;
	IMG_UINT32 ui32CorememCodeFWAddr;
	IMG_UINT32 ui32CorememDataFWAddr;
	IMG_UINT32 ui32CorememCodeSize;
	IMG_UINT32 ui32CorememDataSize;
	IMG_UINT32 ui32Flags;
	IMG_UINT32 ui32Reserved;
} RGXRISCVFW_BOOT_DATA;

#endif /* RGXRISCVFW_ASSEMBLY_CODE */

#endif /* RGX_RISCV_H */
