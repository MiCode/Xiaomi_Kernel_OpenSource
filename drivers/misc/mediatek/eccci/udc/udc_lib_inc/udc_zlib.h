/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

/* zlib.h -- interface of the 'zlib' general purpose compression library
 * version 1.2.11, January 15th, 2017
 *
 * Copyright (C) 1995-2017 Jean-loup Gailly and Mark Adler
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *  2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *  3. This notice may not be removed or altered from any source distribution.
 *
 *  Jean-loup Gailly        Mark Adler
 *  jloup@gzip.org          madler@alumni.caltech.edu
 *
 *
 *  The data format used by the zlib library is described by RFCs (Request for
 *  Comments) 1950 to 1952 in the files http://tools.ietf.org/html/rfc1950
 *  (zlib format), rfc1951 (deflate format) and rfc1952 (gzip format).
 */

#ifndef ZLIB_H
#define ZLIB_H

#ifdef __cplusplus
extern "C" {
#endif

#define ZLIB_VERSION "1.2.11"
#define ZLIB_VERNUM 0x12b0
#define ZLIB_VER_MAJOR 1
#define ZLIB_VER_MINOR 2
#define ZLIB_VER_REVISION 11
#define ZLIB_VER_SUBREVISION 0

struct internal_state;

struct z_stream_s {
	const unsigned char *next_in;     /* next input byte */
	unsigned int avail_in;  /* number of bytes available at next_in */
	unsigned long total_in;  /* total number of input bytes read so far */

	unsigned char *next_out; /* next output byte will go here */
	unsigned int avail_out; /* remaining free space at next_out */
	unsigned long total_out; /* total number of bytes output so far */

	const char *msg;  /* last error message, NULL if no error */
	struct internal_state *state; /* not visible by applications */

	/* used to allocate the internal state */
	void *(*zalloc)(void *opaque, unsigned int items, unsigned int size);

	/* used to free the internal state */
	void (*zfree)(void *opaque, void *address);

	void *opaque;  /* private data object passed to zalloc and zfree */

	/* best guess about the data type: binary or text for deflate,
	 * or the decoding state for inflate
	 */
	int data_type;
	/* Adler-32 or CRC-32 value of the uncompressed data */
	unsigned long adler;
	unsigned long reserved; /* reserved for future use */
};

enum udc_query_id_e {
	UDC_QUERY_WORKSPACE_SIZE = 1,
	UDC_QUERY_SUCCESS = 0,
	UDC_QUERY_NOT_SUPPORT = -1
};

	/* constants */

#define Z_NO_FLUSH      0
#define Z_PARTIAL_FLUSH 1
#define Z_SYNC_FLUSH    2
#define Z_FULL_FLUSH    3
#define Z_FINISH        4
#define Z_BLOCK         5
#define Z_TREES         6
/* Allowed flush values; see deflate() and inflate() below for details */

#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)
/* Return codes for the compression/decompression functions. Negative values
 * are errors, positive values are used for special but normal events.
 */

#define Z_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
/* compression levels */

#define Z_FILTERED            1
#define Z_HUFFMAN_ONLY        2
#define Z_RLE                 3
#define Z_FIXED               4
#define Z_DEFAULT_STRATEGY    0
/* compression strategy; see deflateInit2() below for details */

#define Z_BINARY   0
#define Z_TEXT     1
#define Z_ASCII    Z_TEXT   /* for compatibility with 1.2.2 and earlier */
#define Z_UNKNOWN  2
/* Possible values of the data_type field for deflate() */

#define Z_DEFLATED   8
/* The deflate compression method (the only one supported in this version) */

#define Z_NULL  0  /* for initializing zalloc, zfree, opaque */

#ifdef __cplusplus
}
#endif

#endif /* ZLIB_H */
