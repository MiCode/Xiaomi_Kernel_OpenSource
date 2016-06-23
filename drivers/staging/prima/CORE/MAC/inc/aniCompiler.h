/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*
 * Compiler abstraction layer
 *
 *
 *
 * This file tries to abstract the differences among compilers.
 * Supported compilers are :
 *
 * - GNU C/C++ compiler
 * - Microsoft C/C++ compiler
 * - Intel C/C++ compiler
 *
 * Written by Ho Lee
 */

#ifndef __ANI_COMPILER_ABSTRACT_H
#define __ANI_COMPILER_ABSTRACT_H

/*
 * 1. GNU C/C++ Compiler
 *
 * How to detect gcc : __GNUC__
 * How to detect gcc version : 
 *   major version : __GNUC__ (2 = 2.x, 3 = 3.x, 4 = 4.x)
 *   minor version : __GNUC_MINOR__
 *
 * 2. Microsoft C/C++ Compiler
 *
 * How to detect msc : _MSC_VER
 * How to detect msc version :
 *   _MSC_VER (1200 = MSVC 6.0, 1300 = MSVC 7.0, ...)
 *
 * 3. Intel C/C++ Compiler
 *
 * How to detect icc : __INTEL_COMPILER, __ICC (legacy), __ECC (legacy)
 * How to detect icc version :
 *   __INTEL_COMPILER, __ICC, __ECC (700 = 7.0, 900 = 9.0, ...)
 *
 * 4. Other compilers (not supported)
 *
 * Borland : __BORLANDC__
 * Greenhills : __ghs
 * Metrowerks : __MWERKS__
 * SGI MIPSpro : __sgi
 */

/*
 * Packing directives : These are used to force compiler to pack bits and
 * bytes in the data structure. C standard does not regulate this strictly,
 * and many things are to compiler implementation. Many compilers support
 * compiler specific directives or options that allow different packing
 * and alignment.
 *
 * Alignment directives : Compiler may think packed data structures have
 * no specific alignment requirement. Then compiler may generate multiple
 * byte accesses to access two byte or four bytes data structures. This
 * affects on performance especially for RISC systems. If some data 
 * structure is located on specific alignment always, alignment directives
 * help compiler generate more efficient codes.
 */

#undef __ANI_COMPILER_PRAGMA_PACK_STACK
#undef __ANI_COMPILER_PRAGMA_PACK

#if defined(_MSC_VER)
#define __ANI_COMPILER_PRAGMA_PACK_STACK        1
#define __ANI_COMPILER_PRAGMA_PACK              1
#define __ani_attr_pre_packed 
#define __ani_attr_packed 
#define __ani_attr_aligned_2
#define __ani_attr_aligned_4
#define __ani_attr_aligned_8
#define __ani_attr_aligned_16
#define __ani_attr_aligned_32
#elif defined(__INTEL_COMPILER) || defined(__ICC) || defined(__ECC)
#define __ANI_COMPILER_PRAGMA_PACK              1
#define __ani_attr_pre_packed 
#define __ani_attr_packed 
#define __ani_attr_aligned_2
#define __ani_attr_aligned_4
#define __ani_attr_aligned_8
#define __ani_attr_aligned_16
#define __ani_attr_aligned_32
#elif defined(__GNUC__)
#define __ani_attr_pre_packed 
#define __ani_attr_packed                       __packed
#define __ani_attr_aligned_2                    __attribute__((aligned(2)))
#define __ani_attr_aligned_4                    __attribute__((aligned(4)))
#define __ani_attr_aligned_8                    __attribute__((aligned(8)))
#define __ani_attr_aligned_16                   __attribute__((aligned(16)))
#define __ani_attr_aligned_32                   __attribute__((aligned(32)))
#elif defined(ANI_COMPILER_TYPE_RVCT)
/* Nothing defined so far */
#define __ani_attr_packed
#define __ani_attr_pre_packed                   __packed
#define __ani_attr_aligned_2                    __align(2)
#define __ani_attr_aligned_4                    __align(4)
#define __ani_attr_aligned_8                    __align(8)
#define __ani_attr_aligned_16                   __align(16)
#define __ani_attr_aligned_32                   __align(32)
#else
#error "Unknown compiler"
#endif

#if defined(ANI_DATAPATH_SECTION)
#define  __DP_SRC_RX                __attribute__((section(".dpsrcrx")))
#define  __DP_SRC_TX                __attribute__((section(".dpsrctx")))
#define  __DP_SRC                   __attribute__((section(".dpsrc")))
#define  __ANIHDD_MODULE            __attribute__((section(".anihdd")))
#else
#define  __DP_SRC_RX 
#define  __DP_SRC_TX
#define  __DP_SRC  
#define  __ANIHDD_MODULE
#endif

#endif

