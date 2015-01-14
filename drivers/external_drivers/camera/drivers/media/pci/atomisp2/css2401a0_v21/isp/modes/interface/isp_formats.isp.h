/*
 * Support for Intel Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef _ISP_FORMATS_ISP_H
#define _ISP_FORMATS_ISP_H

#include "assert_support.h"
#include "ia_css_frame_format.h"

/* internal isp representation for the frame formats.
 * always use the encode and decode functions to translate
 * between this format and the enum in ia_css.h
 *
 * to simplify the encode and decode functions these have to be the
 * same as the enum ia_css_frame_format*/

#define	FRAME_FORMAT_NV11 0       /**< 12 bit YUV 411, Y, UV plane */
#define	FRAME_FORMAT_NV12 1       /**< 12 bit YUV 420, Y, UV plane */
#define	FRAME_FORMAT_NV12_TILEY 2 /**< 12 bit YUV 420, Intel proprietary tiled format, TileY */
#define	FRAME_FORMAT_NV16 3       /**< 16 bit YUV 422, Y, UV plane */
#define	FRAME_FORMAT_NV21 4       /**< 12 bit YUV 420, Y, VU plane */
#define	FRAME_FORMAT_NV61 5       /**< 16 bit YUV 422, Y, VU plane */
#define	FRAME_FORMAT_YV12 6       /**< 12 bit YUV 420, Y, V, U plane */
#define	FRAME_FORMAT_YV16 7       /**< 16 bit YUV 422, Y, V, U plane */
#define	FRAME_FORMAT_YUV420    8  /**< 12 bit YUV 420, Y, U, V plane */
#define	FRAME_FORMAT_YUV420_16 9  /**< yuv420, 16 bits per subpixel */
#define	FRAME_FORMAT_YUV422    10 /**< 16 bit YUV 422, Y, U, V plane */
#define	FRAME_FORMAT_YUV422_16 11  /**< yuv422, 16 bits per subpixel */
#define	FRAME_FORMAT_UYVY      12  /**< 16 bit YUV 422, UYVY interleaved */
#define	FRAME_FORMAT_YUYV      13  /**< 16 bit YUV 422, YUYV interleaved */
#define	FRAME_FORMAT_YUV444    14  /**< 24 bit YUV 444, Y, U, V plane */
#define	FRAME_FORMAT_YUV_LINE  15  /**< Internal format, 2 y lines followed
           by a uvinterleaved line */
#define	FRAME_FORMAT_RAW 16	/**< RAW, 1 plane */
#define	FRAME_FORMAT_RGB565 17     /**< 16 bit RGB, 1 plane. Each 3 sub
           pixels are packed into one 16 bit
           value, 5 bits for R, 6 bits for G
           and 5 bits for B. */
#define	FRAME_FORMAT_PLANAR_RGB888 18 /**< 24 bit RGB, 3 planes */
#define	FRAME_FORMAT_RGBA888 19	/**< 32 bit RGBA, 1 plane, A=Alpha
           (alpha is unused) */
#define	FRAME_FORMAT_QPLANE6 20 /**< Internal, for advanced ISP */
#define	FRAME_FORMAT_BINARY_8 21	/**< byte stream, used for jpeg. For
           frames of this type, we set the
           height to 1 and the width to the
           number of allocated bytes. */
#define	FRAME_FORMAT_MIPI 22	/**< MIPI frame, 1 plane */

// PACK_FMT() is used to pack the format identifier to the corresponding bit.
#define PACK_FMT(fmt) (1<<fmt)
#define SUPPORT_FMT(fmt) ((SUPPORTED_OUTPUT_FORMATS & PACK_FMT(fmt)) != 0)

#ifndef PARAM_GENERATION
#include "isp_defs_for_hive.h" /* SUPPORTED_OUTPUT_FORMATS */
#endif

/***************************/

#define SUPPORTS_422      (SUPPORT_FMT(FRAME_FORMAT_NV16)   || SUPPORT_FMT(FRAME_FORMAT_NV61) \
                        || SUPPORT_FMT(FRAME_FORMAT_YUV422) || SUPPORT_FMT(FRAME_FORMAT_YUV422_16) \
                        || SUPPORT_FMT(FRAME_FORMAT_UYVY)   || SUPPORT_FMT(FRAME_FORMAT_YUYV))

#define SUPPORTS_444      (SUPPORT_FMT(FRAME_FORMAT_YUV444))

#define SUPPORTS_RGB      (SUPPORT_FMT(FRAME_FORMAT_RGB565) || SUPPORT_FMT(FRAME_FORMAT_RGBA888) || SUPPORT_FMT(FRAME_FORMAT_PLANAR_RGB888) )

#define SUPPORTS_NV11     (SUPPORT_FMT(FRAME_FORMAT_NV11))

#define SUPPORTS_UV_SWAP  (SUPPORT_FMT(FRAME_FORMAT_NV21) || SUPPORT_FMT(FRAME_FORMAT_NV61))

#define SUPPORTS_UV_IL    (SUPPORT_FMT(FRAME_FORMAT_NV11) || SUPPORT_FMT(FRAME_FORMAT_NV12) \
                        || SUPPORT_FMT(FRAME_FORMAT_NV16) || SUPPORT_FMT(FRAME_FORMAT_NV21) \
                        || SUPPORT_FMT(FRAME_FORMAT_NV61) || SUPPORT_FMT(FRAME_FORMAT_YUV_LINE) \
                        || SUPPORT_FMT(FRAME_FORMAT_UYVY) || SUPPORT_FMT(FRAME_FORMAT_YUYV) \
                        || SUPPORT_FMT(FRAME_FORMAT_NV12_TILEY))

#define SUPPORTS_YUV_IL   (SUPPORT_FMT(FRAME_FORMAT_UYVY) || SUPPORT_FMT(FRAME_FORMAT_YUYV))

#define SUPPORTS_RGB_IL   (SUPPORT_FMT(FRAME_FORMAT_RGBA888) || SUPPORT_FMT(FRAME_FORMAT_RGB565))

#define SUPPORTS_IL       (SUPPORTS_YUV_IL || SUPPORTS_RGB_IL || SUPPORTS_UV_IL)

#define SUPPORTS_UYVY     (SUPPORT_FMT(FRAME_FORMAT_UYVY))

#ifndef PIPE_GENERATION
// encode and decode are used to translate between the host enum, and the isp internal representation.
static inline uint32_t
format_encode(enum ia_css_frame_format fmt) 
{
  OP___assert(FRAME_FORMAT_NV11          == IA_CSS_FRAME_FORMAT_NV11);
  OP___assert(FRAME_FORMAT_NV12          == IA_CSS_FRAME_FORMAT_NV12);
  OP___assert(FRAME_FORMAT_NV12_TILEY    == IA_CSS_FRAME_FORMAT_NV12_TILEY);
  OP___assert(FRAME_FORMAT_NV16          == IA_CSS_FRAME_FORMAT_NV16);
  OP___assert(FRAME_FORMAT_NV21          == IA_CSS_FRAME_FORMAT_NV21);
  OP___assert(FRAME_FORMAT_NV61          == IA_CSS_FRAME_FORMAT_NV61);
  OP___assert(FRAME_FORMAT_YV12          == IA_CSS_FRAME_FORMAT_YV12);
  OP___assert(FRAME_FORMAT_YV16          == IA_CSS_FRAME_FORMAT_YV16);
  OP___assert(FRAME_FORMAT_YUV420        == IA_CSS_FRAME_FORMAT_YUV420);
  OP___assert(FRAME_FORMAT_YUV420_16     == IA_CSS_FRAME_FORMAT_YUV420_16);
  OP___assert(FRAME_FORMAT_YUV422        == IA_CSS_FRAME_FORMAT_YUV422);
  OP___assert(FRAME_FORMAT_YUV422_16     == IA_CSS_FRAME_FORMAT_YUV422_16);
  OP___assert(FRAME_FORMAT_UYVY          == IA_CSS_FRAME_FORMAT_UYVY);
  OP___assert(FRAME_FORMAT_YUYV          == IA_CSS_FRAME_FORMAT_YUYV);
  OP___assert(FRAME_FORMAT_YUV444        == IA_CSS_FRAME_FORMAT_YUV444);
  OP___assert(FRAME_FORMAT_YUV_LINE      == IA_CSS_FRAME_FORMAT_YUV_LINE);
  OP___assert(FRAME_FORMAT_RAW           == IA_CSS_FRAME_FORMAT_RAW);
  OP___assert(FRAME_FORMAT_RGB565        == IA_CSS_FRAME_FORMAT_RGB565);
  OP___assert(FRAME_FORMAT_PLANAR_RGB888 == IA_CSS_FRAME_FORMAT_PLANAR_RGB888);
  OP___assert(FRAME_FORMAT_RGBA888       == IA_CSS_FRAME_FORMAT_RGBA888);
  OP___assert(FRAME_FORMAT_QPLANE6       == IA_CSS_FRAME_FORMAT_QPLANE6);
  OP___assert(FRAME_FORMAT_BINARY_8      == IA_CSS_FRAME_FORMAT_BINARY_8);
  OP___assert(FRAME_FORMAT_MIPI          == IA_CSS_FRAME_FORMAT_MIPI);
  return (uint32_t)fmt;
}

static inline enum ia_css_frame_format
format_decode(uint32_t internal_fmt) 
{
  OP___assert(FRAME_FORMAT_NV11          == IA_CSS_FRAME_FORMAT_NV11);
  OP___assert(FRAME_FORMAT_NV12          == IA_CSS_FRAME_FORMAT_NV12);
  OP___assert(FRAME_FORMAT_NV12_TILEY    == IA_CSS_FRAME_FORMAT_NV12_TILEY);
  OP___assert(FRAME_FORMAT_NV16          == IA_CSS_FRAME_FORMAT_NV16);
  OP___assert(FRAME_FORMAT_NV21          == IA_CSS_FRAME_FORMAT_NV21);
  OP___assert(FRAME_FORMAT_NV61          == IA_CSS_FRAME_FORMAT_NV61);
  OP___assert(FRAME_FORMAT_YV12          == IA_CSS_FRAME_FORMAT_YV12);
  OP___assert(FRAME_FORMAT_YV16          == IA_CSS_FRAME_FORMAT_YV16);
  OP___assert(FRAME_FORMAT_YUV420        == IA_CSS_FRAME_FORMAT_YUV420);
  OP___assert(FRAME_FORMAT_YUV420_16     == IA_CSS_FRAME_FORMAT_YUV420_16);
  OP___assert(FRAME_FORMAT_YUV422        == IA_CSS_FRAME_FORMAT_YUV422);
  OP___assert(FRAME_FORMAT_YUV422_16     == IA_CSS_FRAME_FORMAT_YUV422_16);
  OP___assert(FRAME_FORMAT_UYVY          == IA_CSS_FRAME_FORMAT_UYVY);
  OP___assert(FRAME_FORMAT_YUYV          == IA_CSS_FRAME_FORMAT_YUYV);
  OP___assert(FRAME_FORMAT_YUV444        == IA_CSS_FRAME_FORMAT_YUV444);
  OP___assert(FRAME_FORMAT_YUV_LINE      == IA_CSS_FRAME_FORMAT_YUV_LINE);
  OP___assert(FRAME_FORMAT_RAW           == IA_CSS_FRAME_FORMAT_RAW);
  OP___assert(FRAME_FORMAT_RGB565        == IA_CSS_FRAME_FORMAT_RGB565);
  OP___assert(FRAME_FORMAT_PLANAR_RGB888 == IA_CSS_FRAME_FORMAT_PLANAR_RGB888);
  OP___assert(FRAME_FORMAT_RGBA888       == IA_CSS_FRAME_FORMAT_RGBA888);
  OP___assert(FRAME_FORMAT_QPLANE6       == IA_CSS_FRAME_FORMAT_QPLANE6);
  OP___assert(FRAME_FORMAT_BINARY_8      == IA_CSS_FRAME_FORMAT_BINARY_8);
  OP___assert(FRAME_FORMAT_MIPI          == IA_CSS_FRAME_FORMAT_MIPI);
  return (enum ia_css_frame_format)internal_fmt;
}
#endif

#endif /* _ISP_FORMATS_ISP_H */
