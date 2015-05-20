/* ***********************************************************************************************

  This file is provided under a dual BSD/GPLv2 license.  When using or 
  redistributing this file, you may do so under either license.

  GPL LICENSE SUMMARY

  Copyright(c) 2007-2014 Intel Corporation. All rights reserved.

  This program is free software; you can redistribute it and/or modify 
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation.

  This program is distributed in the hope that it will be useful, but 
  WITHOUT ANY WARRANTY; without even the implied warranty of 
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
  General Public License for more details.

  You should have received a copy of the GNU General Public License 
  along with this program; if not, write to the Free Software 
  Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
  The full GNU General Public License is included in this distribution 
  in the file called LICENSE.GPL.

  BSD LICENSE 

  Copyright(c) 2007-2014 Intel Corporation. All rights reserved.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions 
  are met:

    * Redistributions of source code must retain the above copyright 
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in 
      the documentation and/or other materials provided with the 
      distribution.
    * Neither the name of Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived 
      from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  ***********************************************************************************************
*/

#ifndef _LWPMUDRV_TYPES_H_
#define _LWPMUDRV_TYPES_H_

#if defined(__cplusplus)
extern "C" {
#endif


typedef unsigned char       U8;
typedef          char       S8;
typedef          short      S16;
typedef unsigned short      U16;
typedef unsigned int        U32;
typedef          int        S32;
#if defined(DRV_OS_WINDOWS)
typedef unsigned __int64    U64;
typedef          __int64    S64;
#elif defined (DRV_OS_LINUX) || defined(DRV_OS_SOLARIS) || defined(DRV_OS_MAC) || defined (DRV_OS_ANDROID) || defined(DRV_OS_FREEBSD)
typedef unsigned long long  U64;
typedef          long long  S64;
typedef unsigned long       ULONG;
typedef          void       VOID;
typedef          void*      LPVOID;
#else
#error "Undefined OS"
#endif

#if defined(DRV_IA32)
typedef S32                 SIOP;
typedef U32                 UIOP;
#elif defined(DRV_EM64T)
typedef S64                 SIOP;
typedef U64                 UIOP;
#else
#error "Unexpected Architecture seen"
#endif

typedef U32                 DRV_BOOL;
typedef void*               PVOID;

#if !defined(__DEFINE_STCHAR__)
#define __DEFINE_STCHAR__
#if defined(UNICODE)
typedef wchar_t             STCHAR;
#define VTSA_T(x)           L ## x
#else
typedef char                STCHAR;
#define VTSA_T(x)           x
#endif
#endif

#if defined(DRV_OS_WINDOWS)
#include <wchar.h>
typedef   wchar_t           DRV_STCHAR;
typedef   wchar_t           VTSA_CHAR;
#else
typedef   char              DRV_STCHAR;
#endif

//
// Handy Defines
//
typedef   U32                             DRV_STATUS;

#define   MAX_STRING_LENGTH             1024
#define   MAXNAMELEN                     256

#if defined(DRV_OS_WINDOWS)
#define   UNLINK                        _unlink
#define   RENAME                        rename
#define   WCSDUP                        _wcsdup
#endif
#if defined(DRV_OS_LINUX) || defined(DRV_OS_SOLARIS) || defined(DRV_OS_MAC) || defined (DRV_OS_ANDROID) || defined(DRV_OS_FREEBSD)
#define   UNLINK                        unlink
#define   RENAME                        rename
#endif

#if (defined(DRV_OS_SOLARIS) || defined(DRV_OS_MAC) || defined (DRV_OS_ANDROID) || defined(DRV_OS_FREEBSD))&& !defined(_KERNEL)
//wcsdup is missing on Solaris
#include <stdlib.h>
#include <wchar.h>

static inline wchar_t* solaris_wcsdup(const wchar_t *wc)
{
	wchar_t *tmp = (wchar_t *)malloc((wcslen(wc) + 1) * sizeof(wchar_t));
	wcscpy(tmp, wc);
	return tmp;
}
#define   WCSDUP                        solaris_wcsdup
#endif

#if defined(DRV_OS_LINUX)
#define   WCSDUP                        wcsdup
#endif

#if !defined(_WCHAR_T_DEFINED)
#if defined(DRV_OS_LINUX) || defined(DRV_OS_ANDROID) || defined(DRV_OS_SOLARIS)
#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#endif
#endif

#if (defined(DRV_OS_LINUX) || defined(DRV_OS_ANDROID)) && !defined(__KERNEL__)
#include <wchar.h>
typedef wchar_t VTSA_CHAR;
#endif

#if (defined(DRV_OS_MAC) || defined(DRV_OS_FREEBSD) || defined(DRV_OS_SOLARIS)) && !defined(_KERNEL)
#include <wchar.h>
typedef wchar_t VTSA_CHAR;
#endif


#define   TRUE                          1
#define   FALSE                         0

#define ALIGN_4(x)    (((x) +  3) &  ~3)
#define ALIGN_8(x)    (((x) +  7) &  ~7)
#define ALIGN_16(x)   (((x) + 15) & ~15)
#define ALIGN_32(x)   (((x) + 31) & ~31)

#if defined(__cplusplus)
}
#endif

#endif

