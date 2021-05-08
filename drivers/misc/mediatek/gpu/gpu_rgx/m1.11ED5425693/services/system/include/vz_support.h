/*************************************************************************/ /*!
@File           vz_support.h
@Title          System virtualization support API(s)
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    This header provides the system virtualization API(s)
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

#ifndef _VZ_SUPPORT_H_
#define _VZ_SUPPORT_H_

#include "osfunc.h"
#include "pvrsrv.h"

/*!
******************************************************************************
 @Function			SysVzDevInit

 @Description 		Entry into system virtualization per device configuration

 @Return			PVRSRV_ERROR	PVRSRV_OK on success. Otherwise, a PVRSRV_
									ERROR code
 ******************************************************************************/
PVRSRV_ERROR SysVzDevInit(PVRSRV_DEVICE_CONFIG *psDevConfig);

/*!
******************************************************************************
 @Function			SysVzDevDeInit

 @Description 		Exit from system virtualization per device configuration

 @Return			PVRSRV_ERROR	PVRSRV_OK on success. Otherwise, a PVRSRV_
									ERROR code
 ******************************************************************************/
PVRSRV_ERROR SysVzDevDeInit(PVRSRV_DEVICE_CONFIG *psDevConfig);

/*!
******************************************************************************
 @Function			SysVzCreateDevConfig

 @Description 		Guest para-virtualization initialization per device
					configuration.

 @Return			PVRSRV_ERROR	PVRSRV_OK on success. Otherwise, a PVRSRV_
									ERROR code
 ******************************************************************************/
PVRSRV_ERROR SysVzCreateDevConfig(PVRSRV_DEVICE_CONFIG *psDevConfig);

/*!
******************************************************************************
 @Function			SysVzDestroyDevConfig

 @Description 		Guest para-virtualization deinitialization per device
					configuration.

 @Return			PVRSRV_ERROR	PVRSRV_OK on success. Otherwise, a PVRSRV_
									ERROR code
 ******************************************************************************/
PVRSRV_ERROR SysVzDestroyDevConfig(PVRSRV_DEVICE_CONFIG *psDevConfig);

/*!
******************************************************************************
 @Function			SysVzCreateDevConfig

 @Description 		Server para-virtz handler for client SysVzCreateDevConfig

 @Return			PVRSRV_ERROR	PVRSRV_OK on success. Otherwise, a PVRSRV_
									ERROR code
 ******************************************************************************/
PVRSRV_ERROR SysVzPvzCreateDevConfig(IMG_UINT32 ui32OSID,
									 IMG_UINT32 ui32DevID,
									 IMG_UINT32 *pui32IRQ,
									 IMG_UINT32 *pui32RegsSize,
									 IMG_UINT64 *pui64RegsPAddr);

/*!
******************************************************************************
 @Function			SysVzDestroyDevConfig

 @Description 		Server para-virtz handler for client SysVzDestroyDevConfig

 @Return			PVRSRV_ERROR	PVRSRV_OK on success. Otherwise, a PVRSRV_
									ERROR code
 ******************************************************************************/
PVRSRV_ERROR SysVzPvzDestroyDevConfig(IMG_UINT32 ui32OSID, IMG_UINT32 ui32DevID);

#endif /* _VZ_SUPPORT_H_ */

/*****************************************************************************
 End of file (vz_support.h)
*****************************************************************************/
