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

#if !defined( __I_VOS_TYPES_H )
#define __I_VOS_TYPES_H
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/completion.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <asm/div64.h>

/**=========================================================================
  
  \file  i_vos_Types.h
  
  \brief virtual Operating System Servies (vOSS) Types
               
   Linux specific basic type definitions 
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
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
 */
   
// MACROs to help with compiler and OS specifics.  
// \note: may need to get a little more sophisticated than this and define
// these to specific 'VERSIONS' of the compiler and OS.  Until we have a 
// need for that, lets go with this.
#if defined( _MSC_VER )

#define VOS_COMPILER_MSC
#define VOS_OS_WINMOBILE    // assuming that if we build with MSC, OS is WinMobile

#elif defined( __GNUC__ )

#define VOS_COMPILER_GNUC
#define VOS_OS_LINUX       // assuming if building with GNUC, OS is Linux

#endif


// VOS definitions (compiler specific) for Packing structures.  Note that the 
// Windows compiler defines a way to pack a 'range' of code in a file.  To 
// accomodate this, we have to include a file that has the packing #pragmas
// These files are called
// vos_pack_range_n_start.h where "n" is the packing aligment.  For example,
// vos_pack_range_2_start.h is included in the file where you want to 
// start packing on a 2 byte alignment.  vos_pack_range_end.h is included
// in the file where you want to stop the packing.
//
// Other compilers allow packing individual strucutres so we have a series
// of macros that are added to the structure to define the packing attributes.
// For example, VOS_PACK_STRUCT_2 will add the attributes to pack an 
// individual structure on a 2 byte boundary.  
//
// So what does a coder have to do to properly pack a structure for all the
// supported compilers?  You have to add two includes around *all* the 
// structures you want packed the same way and you also have to add the 
// VOS_PACK_STRUCT_n macros to the individual structures.  
//
// For example to properly pack myStruct on a 2 byte boundary for all 
// voss supported compilers, the following needs coded...
//
//
// #include <vos_pack_range_2_start.h>
//
// typedef struct
// {
//   unsigned char c;
//   long int i;
// } myStruct VOS_PACK_STRUCT_2;
//
//
// note... you can include other structure definitions in here that have the 
// same 2 byte packing
//
// #include <vos_pack_range_end.h>


// /todo: not sure what the flag is to identify the Microsoft compiler for WinMobile
// Let's leave this out for now and just include the defintions for WinMobile.  Need
// to address this when we move to support other operating systems.  Probably best to
// define some of our own 'types' or preprocessor flags like VOS_COMPILER_TYPE, 
// VOS_OS_TYPE, etc. and then all our code can base on those flags/types independent
// of the operating system, compiler, etc.
#if defined( VOS_COMPILER_MSC )


#define VOS_INLINE_FN  __inline

// does nothing on Windows.  packing individual structs is not 
// supported on the Windows compiler.
#define VOS_PACK_STRUCT_1
#define VOS_PACK_STRUCT_2
#define VOS_PACK_STRUCT_4
#define VOS_PACK_STRUCT_8
#define VOS_PACK_STRUCT_16

#elif defined( VOS_COMPILER_GNUC )

#define VOS_INLINE_FN  static inline

#else
#error "Compiling with an unknown compiler!!"
#endif

 

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

/// unsigned 8-bit types
typedef u8 v_U8_t;
typedef u8 v_UCHAR_t;
typedef u8 v_BYTE_t;

/// unsigned 16-bit types
typedef u16 v_U16_t;
typedef unsigned short v_USHORT_t;

/// unsigned 32-bit types
typedef u32 v_U32_t;
// typedef atomic_t v_U32AT_t;
typedef unsigned long  v_ULONG_t;

/// unsigned 64-bit types
typedef u64 v_U64_t;

/// unsigned integer types
typedef unsigned int  v_UINT_t;

/// signed 8-bit types
typedef s8  v_S7_t;
typedef signed char  v_SCHAR_t;

/// signed 16-bit types
typedef s16 v_S15_t;
typedef signed short v_SSHORT_t;

/// signed 32-bit types
typedef s32 v_S31_t;
typedef signed long v_SLONG_t;

/// signed integer types
typedef signed int   v_SINT_t;
                              
/// Boolean types
typedef unsigned char v_BOOL_t;

/// void type
#define v_VOID_t void

#endif // __I_VOSS_TYPES_H
