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

#if !defined( __VOS_PACK_ALIGN_H )
#define __VOS_PACK_ALIGN_H

/**=========================================================================
  
  \file  vos_pack_align.h
  
  \brief virtual Operating System Servies (vOS) pack and align primitives
               
   Definitions for platform independent means of packing and aligning
   data structures
  
   Copyright 2009 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/*
 
  Place the macro VOS_PACK_START above a structure declaration to pack. We 
  are not going to allow modifying the pack size because pack size cannot be 
  specified in AMSS and GNU. Place the macro VOS_PACK_END below a structure 
  declaration to stop the pack. This requirement is necessitated by Windows
  which need pragma based prolog and epilog.

  Pack-size > 1-byte is not supported since gcc and arm do not support that.
  
  Here are some examples
  
  1. Pack-size 1-byte foo_t across all platforms  
  
  VOS_PACK_START
  typedef VOS_PACK_PRE struct foo_s { ... } VOS_PACK_POST foo_t; 
  VOS_PACK_END
  
  2. 2-byte alignment for foo_t across all platforms
  
  typedef VOS_ALIGN_PRE(2) struct foo_s { ... } VOS_ALIGN_POST(2) foo_t; 

  3. Pack-size 1-byte and 2-byte alignment for foo_t across all platforms

  VOS_PACK_START
  typedef VOS_PACK_PRE VOS_ALIGN_PRE(2) struct foo_s { ... } VOS_ALIGN_POST(2) VOS_PACK_POST foo_t; 
  VOS_PACK_END
  
*/

#if defined __GNUC__

  #define VOS_PACK_START 
  #define VOS_PACK_END 

  #define VOS_PACK_PRE 
  #define VOS_PACK_POST  __attribute__((__packed__))

  #define VOS_ALIGN_PRE(__value)
  #define VOS_ALIGN_POST(__value)  __attribute__((__aligned__(__value)))

#elif defined __arm

  #define VOS_PACK_START 
  #define VOS_PACK_END 

  #define VOS_PACK_PRE  __packed
  #define VOS_PACK_POST

  #define VOS_ALIGN_PRE(__value)  __align(__value)
  #define VOS_ALIGN_POST(__value)

#elif defined _MSC_VER

#define VOS_PACK_START  __pragma(pack(push,1))
#define VOS_PACK_END  __pragma(pack(pop))

  #define VOS_PACK_PRE 
  #define VOS_PACK_POST

  #define VOS_ALIGN_PRE(__value)  __declspec(align(__value))
  #define VOS_ALIGN_POST(__value)

#else

  #error Unsupported compiler!!!

#endif

#endif // __VOSS_PACK_ALIGN_H
