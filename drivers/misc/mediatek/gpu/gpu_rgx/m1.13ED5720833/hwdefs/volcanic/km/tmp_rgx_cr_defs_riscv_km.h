/*************************************************************************/ /*!
@Title          Hardware definition file tmp_rgx_cr_defs_riscv_km.h
@Brief          The file contains TEMPORARY hardware definitions without
                BVNC-specific compile time conditionals.
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

#ifndef TMP_RGX_CR_DEFS_RISCV_KM_H
#define TMP_RGX_CR_DEFS_RISCV_KM_H

#if !defined(IMG_EXPLICIT_INCLUDE_HWDEFS)
#error This file may only be included if explicitly defined
#endif

#include "img_types.h"
#include "img_defs.h"


/*
    Register TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG
*/
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG                     (0x3000U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_MASKFULL            (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP1_CONFIG                     (0x3008U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP1_CONFIG_MASKFULL            (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP2_CONFIG                     (0x3010U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP2_CONFIG_MASKFULL            (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP3_CONFIG                     (0x3018U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP3_CONFIG_MASKFULL            (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP4_CONFIG                     (0x3020U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP4_CONFIG_MASKFULL            (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP5_CONFIG                     (0x3028U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP5_CONFIG_MASKFULL            (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP6_CONFIG                     (0x3030U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP6_CONFIG_MASKFULL            (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP7_CONFIG                     (0x3038U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP7_CONFIG_MASKFULL            (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP8_CONFIG                     (0x3040U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP8_CONFIG_MASKFULL            (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP9_CONFIG                     (0x3048U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP9_CONFIG_MASKFULL            (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP10_CONFIG                    (0x3050U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP10_CONFIG_MASKFULL           (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP11_CONFIG                    (0x3058U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP11_CONFIG_MASKFULL           (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP12_CONFIG                    (0x3060U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP12_CONFIG_MASKFULL           (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP13_CONFIG                    (0x3068U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP13_CONFIG_MASKFULL           (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP14_CONFIG                    (0x3070U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP14_CONFIG_MASKFULL           (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP15_CONFIG                    (0x3078U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP15_CONFIG_MASKFULL           (IMG_UINT64_C(0x003FFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_TRUSTED_SHIFT       (62)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_TRUSTED_CLRMSK      (IMG_UINT64_C(0xBFFFFFFFFFFFFFFE))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_TRUSTED_EN          (IMG_UINT64_C(0x4000000000000000))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_LOAD_STORE_SHIFT    (61)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_LOAD_STORE_CLRMSK   (IMG_UINT64_C(0xDFFFFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_LOAD_STORE_EN       (IMG_UINT64_C(0x2000000000000000))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_FETCH_SHIFT         (60)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_FETCH_CLRMSK        (IMG_UINT64_C(0xEFFFFFFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_FETCH_EN            (IMG_UINT64_C(0x1000000000000000))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_REGION_SIZE_SHIFT   (44)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_REGION_SIZE_CLRMSK  (IMG_UINT64_C(0xF0000FFFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_REGION_SIZE_ALIGN   (12U)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_MMU_CONTEXT_SHIFT   (40)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_MMU_CONTEXT_CLRMSK  (IMG_UINT64_C(0xFFFFF0FFFFFFFFFF))
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_DEVVADDR_SHIFT      (12)
#define TMP_RGX_CR_FWCORE_ADDR_REMAP0_CONFIG_DEVVADDR_CLRMSK     (IMG_UINT64_C(0xFFFFFF0000000FFF))


/*
    Register TMP_RGX_CR_FWCORE_BOOT
 */
#define TMP_RGX_CR_FWCORE_BOOT                                   (0x3090U)
#define TMP_RGX_CR_FWCORE_BOOT_BOOT_SHIFT                        (0)
#define TMP_RGX_CR_FWCORE_BOOT_BOOT_CLRMSK                       (IMG_UINT64_C(0x0000000000000000))
#define TMP_RGX_CR_FWCORE_BOOT_BOOT_EN                           (IMG_UINT64_C(0x0000000000000001))


/*
    Register TMP_RGX_CR_FWCORE_RESET_ADDR
 */
#define TMP_RGX_CR_FWCORE_RESET_ADDR                             (0x3098U)
#define TMP_RGX_CR_FWCORE_RESET_ADDR_ADDR_SHIFT                  (1)
#define TMP_RGX_CR_FWCORE_RESET_ADDR_ADDR_CLRMSK                 (IMG_UINT64_C(0x0000000000000000))


/*
    Register TMP_RGX_CR_FWCORE_WRAPPER_FENCE
 */
#define TMP_RGX_CR_FWCORE_WRAPPER_FENCE                          (0x30E8U)
#define TMP_RGX_CR_FWCORE_WRAPPER_FENCE_FENCE_SHIFT              (0)
#define TMP_RGX_CR_FWCORE_WRAPPER_FENCE_FENCE_CLRMSK             (IMG_UINT64_C(0x0000000000000000))


/*
    Register TMP_RGX_CR_MTIME_SET
*/
#define TMP_RGX_CR_MTIME_SET                                     (0x7000U)
#define TMP_RGX_CR_MTIME_SET_MASKFULL                            (IMG_UINT64_C(0xFFFFFFFFFFFFFFFF))

/*
    Register TMP_RGX_CR_MTIME_CMP
*/
#define TMP_RGX_CR_MTIME_CMP                                     (0x7008U)
#define TMP_RGX_CR_MTIME_CMP_MASKFULL                            (IMG_UINT64_C(0xFFFFFFFFFFFFFFFF))

/*
    Register TMP_RGX_CR_MTIME_CTRL
*/
#define TMP_RGX_CR_MTIME_CTRL                                    (0x7018U)
#define TMP_RGX_CR_MTIME_CTRL_MASKFULL                           (IMG_UINT64_C(0x0000000080000003))
#define TMP_RGX_CR_MTIME_CTRL_SOFT_RESET_SHIFT                   (31)
#define TMP_RGX_CR_MTIME_CTRL_SOFT_RESET_CLRMSK                  (IMG_UINT64_C(0x0000000000000003))
#define TMP_RGX_CR_MTIME_CTRL_SOFT_RESET_EN                      (IMG_UINT64_C(0x0000000080000000))
#define TMP_RGX_CR_MTIME_CTRL_PAUSE_SHIFT                        (1)
#define TMP_RGX_CR_MTIME_CTRL_PAUSE_CLRMSK                       (IMG_UINT64_C(0x0000000080000001))
#define TMP_RGX_CR_MTIME_CTRL_PAUSE_EN                           (IMG_UINT64_C(0x0000000000000002))
#define TMP_RGX_CR_MTIME_CTRL_ENABLE_SHIFT                       (0)
#define TMP_RGX_CR_MTIME_CTRL_ENABLE_CLRMSK                      (IMG_UINT64_C(0x0000000080000002))
#define TMP_RGX_CR_MTIME_CTRL_ENABLE_EN                          (IMG_UINT64_C(0x0000000000000001))

#endif /* TMP_RGX_CR_DEFS_RISCV_KM_H */

/*****************************************************************************
 End of file (tmp_rgx_cr_defs_riscv_km.h)
*****************************************************************************/
