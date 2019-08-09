/*************************************************************************/ /*!
@File
@Title          Wrapper around drm_fourcc.h
@Description    FourCCs and DRM framebuffer modifiers that are not in the
                Kernel's and libdrm's drm_fourcc.h can be added here.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        MIT

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/ /**************************************************************************/

#ifndef IMG_DRM_FOURCC_H
#define IMG_DRM_FOURCC_H

#if defined(__KERNEL__)
#include <drm/drm_fourcc.h>
#else
/*
 * Include types.h to workaround versions of libdrm older than 2.4.68
 * not including the correct headers.
 */
#include <linux/types.h>

#include <drm_fourcc.h>
#endif

/*
 * Don't get too inspired by this example :)
 * ADF doesn't support DRM modifiers, so the memory layout had to be
 * included in the fourcc name, but the proper way to specify information
 * additional to pixel formats is to use DRM modifiers.
 *
 * See upstream drm_fourcc.h for the proper naming convention.
 */
#ifndef DRM_FORMAT_BGRA8888_DIRECT_16x4
#define DRM_FORMAT_BGRA8888_DIRECT_16x4 fourcc_code('I', 'M', 'G', '0')
#endif

/*
 * Upstream doesn't have a floating point format yet, so let's make one
 * up.
 * Note: The kernel's core DRM needs to know about this format,
 * otherwise it won't be supported and should not be exposed by our
 * kernel modules either.
 * Refer to the provided kernel patch adding this format.
 */
#if !defined(__KERNEL__)
#define DRM_FORMAT_ABGR16_IMG fourcc_code('I', 'M', 'G', '1')
#endif

/*
 * Value chosen in the middle of 255 pool to minimise the chance of hitting
 * the same value potentially defined by other vendors in the drm_fourcc.h
 */
#define DRM_FORMAT_MOD_VENDOR_PVR 0x92

#ifndef DRM_FORMAT_MOD_VENDOR_NONE
#define DRM_FORMAT_MOD_VENDOR_NONE 0
#endif

#ifndef DRM_FORMAT_RESERVED
#define DRM_FORMAT_RESERVED ((1ULL << 56) - 1)
#endif

#ifndef fourcc_mod_code
#define fourcc_mod_code(vendor, val) \
	((((__u64)DRM_FORMAT_MOD_VENDOR_## vendor) << 56) | (val & 0x00ffffffffffffffULL))
#endif

#ifndef DRM_FORMAT_MOD_INVALID
#define DRM_FORMAT_MOD_INVALID fourcc_mod_code(NONE, DRM_FORMAT_RESERVED)
#endif

#ifndef DRM_FORMAT_MOD_LINEAR
#define DRM_FORMAT_MOD_LINEAR fourcc_mod_code(NONE, 0)
#endif

#endif /* IMG_DRM_FOURCC_H */
