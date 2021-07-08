/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Hung-Wen Hsieh <hung-wen.hsieh@mediatek.com>
 */

#ifndef __UFBC_DEF_H__
#define __UFBC_DEF_H__

#include "stdint.h"

/**
 * Describe Mediatek UFBC (Universal Frame Buffer Compression) buffer header.
 * Mediatek UFBC supports 1 plane Bayer and 2 planes Y/UV image formats.
 * Caller must follow the formulation to calculate the bit stream buffer size
 * and length table buffer size.
 *
 * Header Size
 *
 * Fixed size of 4096 bytes. Reserved bytes will be used by Mediatek
 * ISP driver. Caller SHOULD NOT edit it.
 *
 * Bit Stream Size
 *
 *  @code
 *    // for each plane
 *    size = ((width + 63) / 64) * 64;      // width must be aligned to 64 pixel
 *    size = (size * bitsPerPixel + 7) / 8; // convert to bytes
 *    size = size * height;
 *  @endcode
 *
 * Table Size
 *
 *  @code
 *    // for each plane
 *    size = (width + 63) / 64;
 *    size = size * height;
 *  @endcode
 *
 * And the memory layout should be followed as
 *
 *  @code
 *           Bayer                  YUV2P
 *    +------------------+  +------------------+
 *    |      Header      |  |      Header      |
 *    +------------------+  +------------------+
 *    |                  |  |     Y Plane      |
 *    | Bayer Bit Stream |  |    Bit Stream    |
 *    |                  |  |                  |
 *    +------------------+  +------------------+
 *    |   Length Table   |  |     UV Plane     |
 *    +------------------+  |    Bit Stream    |
 *                          |                  |
 *                          +------------------+
 *                          |     Y Plane      |
 *                          |   Length Table   |
 *                          +------------------+
 *                          |     UV Plane     |
 *                          |   Length Table   |
 *                          +------------------+
 *  @endcode
 *
 *  @note Caller has responsibility to fill all the fields according the
 *        real buffer layout.
 */

struct UfbcBufferHeader {
	union {
		struct {
			/** Describe image resolution, unit in pixel. */
			uint32_t width;

			/** Describe image resolution, unit in pixel. */
			uint32_t height;

			/** Describe UFBC data plane count, UFBC supports maximum 2 planes. */
			uint32_t planeCount;

			/** Describe the original image data bits per pixel of the given plane. */
			uint32_t bitsPerPixel[3];

			/**
			 * Describe the offset of the given plane bit stream data in bytes,
			 * including header size.
			 */
			uint32_t bitstreamOffset[3];

			/** Describe the bit stream data size in bytes of the given plane. */
			uint32_t bitStreamSize[3];

			/** Describe the encoded data size in bytes of the given plane. */
			uint32_t bitStreamDataSize[3];

			/**
			 * Describe the offset of length table of the given plane, including
			 * header size.
			 */
			uint32_t tableOffset[3];

			/** Describe the length table size of the given plane */
			uint32_t tableSize[3];

			/** Describe the total buffer size, including buffer header. */
			uint32_t bufferSize;
		};
		uint8_t reserved[4096];
	};
} __attributes__((packed));

struct _IMG_META_INFO {
	unsigned int Version;
	unsigned int HeaderSize;
	unsigned int BitStreamOffset[8];
	unsigned int LengthTableOffset[8];
} IMG_META_INFO;

/******************************************************************************
 * @UFBC format meta info
 *
 ******************************************************************************/
struct _UFD_META_INFO {
	unsigned int bUF;
	unsigned int UFD_BITSTREAM_OFST_ADDR;
	unsigned int UFD_BS_AU_START;
	unsigned int UFD_AU2_SIZE;
	unsigned int UFD_BOND_MODE;
} UFD_META_INFO;

struct _UFD_HW_META_INFO {
	unsigned int Buf[32];
} UFD_HW_META_INFO;

union UFDStruct {
	UFD_META_INFO UFD;
	UFD_HW_META_INFO HWUFD;
};

struct _UFO_META_INFO {
	struct UfbcBufferHeader ImgInfo;
	unsigned int AUWriteBySW;
	union UFDStruct UFD;
} UFO_META_INFO;


struct _YUFD_META_INFO {
	unsigned int bYUF;
	unsigned int YUFD_BITSTREAM_OFST_ADDR;
	unsigned int YUFD_BS_AU_START;
	unsigned int YUFD_AU2_SIZE;
	unsigned int YUFD_BOND_MODE;
} YUFD_META_INFO;

struct _YUFD_HW_META_INFO {
	unsigned int Buf[32];
} YUFD_HW_META_INFO;

union YUFDStruct {
	YUFD_META_INFO YUFD;
	YUFD_HW_META_INFO HWYUFD;
};

struct _YUFO_META_INFO {
	struct UfbcBufferHeader ImgInfo;
	unsigned int AUWriteBySW;
	union YUFDStruct YUFD;
} YUFO_META_INFO;

#endif // __UFBC_DEF_H__
