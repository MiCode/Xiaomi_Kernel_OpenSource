/*************************************************************************/ /*!
@File			pdumpdesc.h
@Title          PDump Descriptor format
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Describes PDump descriptors that may be passed to the
				extraction routines (SAB).
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

#if !defined (__PDUMPDESC_H__)
#define __PDUMPDESC_H__

#include "pdumpdefs.h"

/*
 * Common fields
 */
#define HEADER_WORD0_TYPE_SHIFT				(0)
#define HEADER_WORD0_TYPE_CLRMSK			(0xFFFFFFFFU)

#define HEADER_WORD1_SIZE_SHIFT				(0)
#define HEADER_WORD1_SIZE_CLRMSK			(0x0000FFFFU)
#define HEADER_WORD1_VERSION_SHIFT			(16)
#define HEADER_WORD1_VERSION_CLRMSK			(0xFFFF0000U)

#define HEADER_WORD2_DATA_SIZE_SHIFT		(0)
#define HEADER_WORD2_DATA_SIZE_CLRMSK		(0xFFFFFFFFU)


/*
 * The image type descriptor
 */

/*
 * Header type (IMGBv2) - 'IMGB' in hex + VERSION 1
 * Header size - 56 bytes
 */
#define IMAGE_HEADER_TYPE					(0x42474D49)
#define IMAGE_HEADER_SIZE					(56)
#define IMAGE_HEADER_VERSION				(1)

/*
 * Image type-specific fields
 */
#define IMAGE_HEADER_WORD3_LOGICAL_WIDTH_SHIFT		(0)
#define IMAGE_HEADER_WORD3_LOGICAL_WIDTH_CLRMSK		(0xFFFFFFFFU)

#define IMAGE_HEADER_WORD4_LOGICAL_HEIGHT_SHIFT		(0)
#define IMAGE_HEADER_WORD4_LOGICAL_HEIGHT_CLRMSK	(0xFFFFFFFFU)

#define IMAGE_HEADER_WORD5_FORMAT_SHIFT				(0)
#define IMAGE_HEADER_WORD5_FORMAT_CLRMSK			(0xFFFFFFFFU)

#define IMAGE_HEADER_WORD6_PHYSICAL_WIDTH_SHIFT		(0)
#define IMAGE_HEADER_WORD6_PHYSICAL_WIDTH_CLRMSK	(0xFFFFFFFFU)

#define IMAGE_HEADER_WORD7_PHYSICAL_HEIGHT_SHIFT	(0)
#define IMAGE_HEADER_WORD7_PHYSICAL_HEIGHT_CLRMSK	(0xFFFFFFFFU)

#define IMAGE_HEADER_WORD8_TWIDDLING_SHIFT			(0)
#define IMAGE_HEADER_WORD8_TWIDDLING_CLRMSK			(0x000000FFU)
#define IMAGE_HEADER_WORD8_TWIDDLING_STRIDED		(0 << IMAGE_HEADER_WORD8_TWIDDLING_SHIFT)
#define IMAGE_HEADER_WORD8_TWIDDLING_ZTWIDDLE		(12 << IMAGE_HEADER_WORD8_TWIDDLING_SHIFT)


#define IMAGE_HEADER_WORD8_STRIDE_SHIFT				(8)
#define IMAGE_HEADER_WORD8_STRIDE_CLRMSK			(0x0000FF00U)
#define IMAGE_HEADER_WORD8_STRIDE_POSITIVE			(0 << IMAGE_HEADER_WORD8_STRIDE_SHIFT)
#define IMAGE_HEADER_WORD8_STRIDE_NEGATIVE			(1 << IMAGE_HEADER_WORD8_STRIDE_SHIFT)

#define IMAGE_HEADER_WORD8_BIFTYPE_SHIFT			(16)
#define IMAGE_HEADER_WORD8_BIFTYPE_CLRMSK			(0x00FF0000U)
#define IMAGE_HEADER_WORD8_BIFTYPE_NONE				(0 << IMAGE_HEADER_WORD8_BIFTYPE_SHIFT)

#define IMAGE_HEADER_WORD8_FBCTYPE_SHIFT			(24)
#define IMAGE_HEADER_WORD8_FBCTYPE_CLRMSK			(0xFF000000U)
#define IMAGE_HEADER_WORD8_FBCTYPE_8X8				(1 << IMAGE_HEADER_WORD8_FBCTYPE_SHIFT)
#define IMAGE_HEADER_WORD8_FBCTYPE_16x4				(2 << IMAGE_HEADER_WORD8_FBCTYPE_SHIFT)

#define IMAGE_HEADER_WORD9_FBCDECOR_SHIFT			(0)
#define IMAGE_HEADER_WORD9_FBCDECOR_CLRMSK			(0x000000FFU)
#define IMAGE_HEADER_WORD9_FBCDECOR_ENABLE			(1 << IMAGE_HEADER_WORD9_FBCDECOR_SHIFT)

#define IMAGE_HEADER_WORD9_FBCCOMPAT_SHIFT			(8)
#define IMAGE_HEADER_WORD9_FBCCOMPAT_CLRMSK			(0x0000FF00U)
#define IMAGE_HEADER_WORD9_FBCCOMPAT_SAME_AS_GPU 	(0 << IMAGE_HEADER_WORD9_FBCCOMPAT_SHIFT)
#define IMAGE_HEADER_WORD9_FBCCOMPAT_BASE 			(1 << IMAGE_HEADER_WORD9_FBCCOMPAT_SHIFT)
#define IMAGE_HEADER_WORD9_FBCCOMPAT_TWIDDLED_EN	(2 << IMAGE_HEADER_WORD9_FBCCOMPAT_SHIFT)
#define IMAGE_HEADER_WORD9_FBCCOMPAT_V2				(3 << IMAGE_HEADER_WORD9_FBCCOMPAT_SHIFT)
#define IMAGE_HEADER_WORD9_FBCCOMPAT_V3				(4 << IMAGE_HEADER_WORD9_FBCCOMPAT_SHIFT)
#define IMAGE_HEADER_WORD9_FBCCOMPAT_V3_REMAP		(5 << IMAGE_HEADER_WORD9_FBCCOMPAT_SHIFT)

#define IMAGE_HEADER_WORD10_FBCCLEAR_CH0_SHIFT		(0)
#define IMAGE_HEADER_WORD10_FBCCLEAR_CH0_CLRMSK		(0xFFFFFFFFU)

#define IMAGE_HEADER_WORD11_FBCCLEAR_CH1_SHIFT		(0)
#define IMAGE_HEADER_WORD11_FBCCLEAR_CH1_CLRMSK		(0xFFFFFFFFU)

#define IMAGE_HEADER_WORD12_FBCCLEAR_CH2_SHIFT		(0)
#define IMAGE_HEADER_WORD12_FBCCLEAR_CH2_CLRMSK		(0xFFFFFFFFU)

#define IMAGE_HEADER_WORD13_FBCCLEAR_CH3_SHIFT		(0)
#define IMAGE_HEADER_WORD13_FBCCLEAR_CH3_CLRMSK		(0xFFFFFFFFU)


#endif /* __PDUMPDESC_H__ */

