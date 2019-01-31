/*************************************************************************/ /*!
@File           img_drm_fourcc.h
@Title          Wrapper around drm_fourcc.h
@Description    FourCCs that are not in the Kernel's and libdrm's drm_fourcc.h
                can be added here, whether they are private or just not in the
                upstream version currently used.
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

#endif /* IMG_DRM_FOURCC_H */
