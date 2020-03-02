/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __UDC_H__
#define __UDC_H__

#include <linux/printk.h>
#include <linux/stddef.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/gfp.h>
#include "udc_zlib.h"
#include "udc_dictionary.h"

enum UDC_FUNC_ID {
	ID_deflateInit2,
	ID_deflateSetDict,
	ID_deflateEnd,
	ID_deflateReset,
	ID_deflate,
	ID_deflateBound,
	ID_udc_chksum,
	ID_udc_QueryPara,
	ID_udc_GetCmpLen,
};

typedef int (*deflateInit2_cb_func_t) (struct z_stream_s *,
	int, int, int, int, int, const char*, int);

typedef int (*deflateSetDict_cb_func_t) (struct z_stream_s *,
	const unsigned char*, unsigned int);

typedef int (*deflate_cb_func_t)(struct z_stream_s *, int);

typedef int (*deflateEnd_cb_func_t)(struct z_stream_s *);

typedef int (*deflateReset_cb_func_t)(struct z_stream_s *);

typedef unsigned long (*deflateBound_cb_func_t)
	(struct z_stream_s *, unsigned long);

typedef unsigned int (*udc_chksum_cb_func_t)(struct z_stream_s *);

typedef int (*udc_QueryPara_cb_func_t)(struct z_stream_s *, int, void *);

typedef unsigned int (*udc_GetCmpLen_cb_func_t)
	(struct z_stream_s *, unsigned char*, unsigned char*);

struct udc_func_info {
	deflateInit2_cb_func_t deflateInit2;
	deflateSetDict_cb_func_t deflateSetDict;
	deflate_cb_func_t deflate;
	deflateEnd_cb_func_t deflateEnd;
	deflateReset_cb_func_t deflateReset;
	deflateBound_cb_func_t deflateBound;
	udc_chksum_cb_func_t udc_chksum;
	udc_QueryPara_cb_func_t udc_QueryPara;
	udc_GetCmpLen_cb_func_t udc_GetCmpLen;
};

struct udc_private_data {
	unsigned char *mem;
	unsigned int size;
	unsigned int used;
};

int udc_init(struct z_stream_s *zcpr, struct udc_private_data *my_param);
void udc_deinit(struct z_stream_s *zcpr);

int deflateInit2_cb(struct z_stream_s *strm, int level, int method,
			int windowBits, int memLevel, int strategy);
int deflateSetDictionary_cb(struct z_stream_s *strm, const char *dictionary,
				unsigned int dictLength);

int deflate_cb(struct z_stream_s *strm, int flush);
int deflateEnd_cb(struct z_stream_s *strm);
int deflateReset_cb(struct z_stream_s *strm);
int deflateBound_cb(struct z_stream_s *strm, unsigned long sourceLen);
int udc_chksum_cb(struct z_stream_s *strm);
int udc_QueryPara_cb(struct z_stream_s *strm, int id, void *param);
int udc_GetCmpLen_cb(struct z_stream_s *strm,
	unsigned char *start, unsigned char *end);

#endif
