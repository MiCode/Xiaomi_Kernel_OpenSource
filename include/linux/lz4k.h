/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef __LZ4K_H__
#define __LZ4K_H__

#include <linux/types.h>
#include <linux/lzo.h>

#define LZ4K_TAG 1261722188	/* "LZ4K" */

#ifndef CONFIG_64BIT
#define LZ4K_MEM_COMPRESS LZO1X_MEM_COMPRESS
#else
#define LZ4K_MEM_COMPRESS (LZO1X_MEM_COMPRESS << 1)
#endif

int lz4k_compress(const unsigned char *src, size_t src_len,
		  unsigned char *dst, size_t *dst_len, void *wrkmem);

int lz4k_decompress_safe(const unsigned char *src, size_t src_len,
			 unsigned char *dst, size_t *dst_len);
int lz4k_compress_zram(const unsigned char *src, size_t src_len,
		  unsigned char *dst, size_t *dst_len, void *wrkmem, int *checksum);
int lz4k_decompress_ubifs(const unsigned char *src, size_t src_len,
			  unsigned char *dst, size_t *dst_len);


#if 0

extern int lz4k_compress_ubifs(const unsigned char *in, size_t in_len, unsigned char *out,
		size_t *out_len, void *wrkmem);

extern int lz4k_decompress_ubifs(const unsigned char *in, size_t in_len,
		unsigned char *out, size_t *out_len);

extern int lz4k_compress(const unsigned char *in, size_t in_len, unsigned char *out,
		size_t *out_len, void *wrkmem);
#endif

#endif				/* __LZ4K_H__ */
