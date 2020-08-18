/*************************************************************************/ /*!
@File
@Title          OS functions header
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    OS specific API definitions
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

#ifndef __OSFUNC_COMMON_H__
/*! @cond Doxygen_Suppress */
#define __OSFUNC_COMMON_H__
/*! @endcond */

#include "img_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if defined(__arm64__) || defined(__aarch64__) || defined(PVRSRV_DEVMEM_TEST_SAFE_MEMSETCPY)

/* Workarounds for assumptions made that memory will not be mapped uncached
 * in kernel or user address spaces on arm64 platforms (or other testing).
 */

/**************************************************************************/ /*!
@Function       DeviceMemSet
@Description    Set memory, whose mapping may be uncached, to a given value.
                On some architectures, additional processing may be needed
                if the mapping is uncached. In such cases, OSDeviceMemSet()
                is defined as a call to this function.
@Input          pvDest     void pointer to the memory to be set
@Input          ui8Value   byte containing the value to be set
@Input          ui32Size   the number of bytes to be set to the given value
@Return         None
 */ /**************************************************************************/
void DeviceMemSet(void *pvDest, IMG_UINT8 ui8Value, size_t ui32Size);

/**************************************************************************/ /*!
@Function       DeviceMemCopy
@Description    Copy values from one area of memory, to another, when one
                or both mappings may be uncached.
                On some architectures, additional processing may be needed
                if mappings are uncached. In such cases, OSDeviceMemCopy()
                is defined as a call to this function.
@Input          pvDst      void pointer to the destination memory
@Input          pvSrc      void pointer to the source memory
@Input          ui32Size   the number of bytes to be copied
@Return         None
 */ /**************************************************************************/
void DeviceMemCopy(void *pvDst, const void *pvSrc, size_t ui32Size);

#define OSDeviceMemSet(a,b,c)  DeviceMemSet((a), (b), (c))
#define OSDeviceMemCopy(a,b,c) DeviceMemCopy((a), (b), (c))
#define OSCachedMemSet(a,b,c)  memset((a), (b), (c))
#define OSCachedMemCopy(a,b,c) memcpy((a), (b), (c))

#else /* !(defined(__arm64__) || defined(__aarch64__) || defined(PVRSRV_DEVMEM_TEST_SAFE_MEMSETCPY)) */

/* Everything else */

/**************************************************************************/ /*!
@Function       OSDeviceMemSet
@Description    Set memory, whose mapping may be uncached, to a given value.
                On some architectures, additional processing may be needed
                if the mapping is uncached.
@Input          a     void pointer to the memory to be set
@Input          b     byte containing the value to be set
@Input          c     the number of bytes to be set to the given value
@Return         Pointer to the destination memory.
 */ /**************************************************************************/
#define OSDeviceMemSet(a,b,c) memset((a), (b), (c))

/**************************************************************************/ /*!
@Function       OSDeviceMemCopy
@Description    Copy values from one area of memory, to another, when one
                or both mappings may be uncached.
                On some architectures, additional processing may be needed
                if mappings are uncached.
@Input          a     void pointer to the destination memory
@Input          b     void pointer to the source memory
@Input          c     the number of bytes to be copied
@Return         Pointer to the destination memory.
 */ /**************************************************************************/
#define OSDeviceMemCopy(a,b,c) memcpy((a), (b), (c))

/**************************************************************************/ /*!
@Function       OSCachedMemSet
@Description    Set memory, where the mapping is known to be cached, to a
                given value. This function exists to allow an optimal memset
                to be performed when memory is known to be cached.
@Input          a     void pointer to the memory to be set
@Input          b     byte containing the value to be set
@Input          c     the number of bytes to be set to the given value
@Return         Pointer to the destination memory.
 */ /**************************************************************************/
#define OSCachedMemSet(a,b,c)  memset((a), (b), (c))

/**************************************************************************/ /*!
@Function       OSCachedMemCopy
@Description    Copy values from one area of memory, to another, when both
                mappings are known to be cached.
                This function exists to allow an optimal memcpy to be
                performed when memory is known to be cached.
@Input          a     void pointer to the destination memory
@Input          b     void pointer to the source memory
@Input          c     the number of bytes to be copied
@Return         Pointer to the destination memory.
 */ /**************************************************************************/
#define OSCachedMemCopy(a,b,c) memcpy((a), (b), (c))

#endif /* !(defined(__arm64__) || defined(__aarch64__) || defined(PVRSRV_DEVMEM_TEST_SAFE_MEMSETCPY)) */

#ifdef __cplusplus
}
#endif

#endif /* __OSFUNC_COMMON_H__ */

/******************************************************************************
 End of file (osfunc_common.h)
******************************************************************************/
