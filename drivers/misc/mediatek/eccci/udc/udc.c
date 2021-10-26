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
#include "udc.h"

struct udc_func_info udc_func_table;

/* Register UDC API */
int register_udc_func_deflateInit2(deflateInit2_cb_func_t func)
{
	int ret = 0;

	if (udc_func_table.deflateInit2 == NULL) {
		udc_func_table.deflateInit2 = func;
		pr_notice("%s success\n", __func__);
	} else {
		pr_notice("%s fail: registered!\n", __func__);
		ret = -1;
	}

	return ret;
}

int deflateInit2_cb(struct z_stream_s *strm,
			int  level, int  method,
			int windowBits, int memLevel,
			int strategy)
{
	deflateInit2_cb_func_t func;
	int ret = 0;

	func = udc_func_table.deflateInit2;
	if (func != NULL) {
		ret = func(strm, level, method, windowBits, memLevel, strategy,
				ZLIB_VERSION, (int)sizeof(struct z_stream_s));
	} else {
		ret = -1;/* E_NO_EXIST */
		pr_notice("exec %s fail: not register!\n", __func__);
	}
	return ret;
}

int register_udc_func_deflateSetDict(
	deflateSetDict_cb_func_t func)
{
	int ret = 0;

	if (udc_func_table.deflateSetDict == NULL) {
		udc_func_table.deflateSetDict = func;
		pr_notice("%s success\n", __func__);
	} else {
		pr_notice("%s fail: registered!\n", __func__);
		ret = -1;
	}

	return ret;
}

int deflateSetDictionary_cb(struct z_stream_s *strm,
	const char *dictionary, unsigned int dictLength)
{
	deflateSetDict_cb_func_t func;
	int ret = 0;

	func = udc_func_table.deflateSetDict;
	if (func != NULL) {
		ret = func(strm, dictionary, dictLength);
	} else {
		ret = -1;/* E_NO_EXIST */
		pr_notice("exec %s fail: not register!\n", __func__);
	}
	return ret;
}

int register_udc_func_deflate(deflate_cb_func_t func)
{
	int ret = 0;

	if (udc_func_table.deflate == NULL) {
		udc_func_table.deflate = func;
		pr_notice("%s success\n", __func__);
	} else {
		pr_notice("%s fail: registered!\n", __func__);
		ret = -1;
	}

	return ret;
}

int deflate_cb(struct z_stream_s *strm, int flush)
{
	deflate_cb_func_t func;
	int ret = 0;

	func = udc_func_table.deflate;
	if (func != NULL) {
		ret = func(strm, flush);
	} else {
		ret = -1;//E_NO_EXIST
		pr_notice("exec %s fail: not register!\n", __func__);
	}
	return ret;
}

int register_udc_func_deflateEnd(deflateEnd_cb_func_t func)
{
	int ret = 0;

	if (udc_func_table.deflateEnd == NULL) {
		udc_func_table.deflateEnd = func;
		pr_notice("%s success\n", __func__);
	} else {
		pr_notice("%s fail: registered!\n", __func__);
		ret = -1;
	}

	return ret;
}

int deflateEnd_cb(struct z_stream_s *strm)
{
	deflateEnd_cb_func_t func;
	int ret = 0;

	func = udc_func_table.deflateEnd;
	if (func != NULL) {
		ret = func(strm);
	} else {
		ret = -1;//E_NO_EXIST
		pr_notice("exec %s fail: not register!\n", __func__);
	}
	return ret;
}

int register_udc_func_deflateReset(deflateReset_cb_func_t func)
{
	int ret = 0;

	if (udc_func_table.deflateReset == NULL) {
		udc_func_table.deflateReset = func;
		pr_notice("%s success\n", __func__);
	} else {
		pr_notice("%s fail: registered!\n", __func__);
		ret = -1;
	}

	return ret;
}

int deflateReset_cb(struct z_stream_s *strm)
{
	deflateReset_cb_func_t func;
	int ret = 0;

	func = udc_func_table.deflateReset;
	if (func != NULL) {
		ret = func(strm);
	} else {
		ret = -1;//E_NO_EXIST
		pr_notice("exec %s fail: not register!\n", __func__);
	}
	return ret;
}

int register_udc_func_deflateBound(deflateBound_cb_func_t func)
{
	int ret = 0;

	if (udc_func_table.deflateBound == NULL) {
		udc_func_table.deflateBound = func;
		pr_notice("%s success\n", __func__);
	} else {
		pr_notice("%s fail: registered!\n", __func__);
		ret = -1;
	}

	return ret;
}

int deflateBound_cb(struct z_stream_s *strm, unsigned long sourceLen)
{
	deflateBound_cb_func_t func;
	int ret = 0;

	func = udc_func_table.deflateBound;
	if (func != NULL) {
		ret = func(strm, sourceLen);
	} else {
		ret = -1;//E_NO_EXIST
		pr_notice("exec %s fail: not register!\n", __func__);
	}
	return ret;
}

int register_udc_func_udc_chksum(udc_chksum_cb_func_t func)
{
	int ret = 0;

	if (udc_func_table.udc_chksum == NULL) {
		udc_func_table.udc_chksum = func;
		pr_notice("%s success\n", __func__);
	} else {
		pr_notice("%s fail: registered!\n", __func__);
		ret = -1;
	}

	return ret;
}

int udc_chksum_cb(struct z_stream_s *strm)
{
	udc_chksum_cb_func_t func;
	unsigned int ret = 0;

	func = udc_func_table.udc_chksum;
	if (func != NULL) {
		ret = func(strm);
	} else {
		ret = -1;//E_NO_EXIST
		pr_notice("exec %s fail: not register!\n", __func__);
	}
	return ret;
}

int register_udc_func_udc_QueryPara(udc_QueryPara_cb_func_t func)
{
	int ret = 0;

	if (udc_func_table.udc_QueryPara == NULL) {
		udc_func_table.udc_QueryPara = func;
		pr_notice("%s success\n", __func__);
	} else {
		pr_notice("%s fail: registered!\n", __func__);
		ret = -1;
	}

	return ret;
}

int udc_QueryPara_cb(struct z_stream_s *strm, int id, void *param)
{
	udc_QueryPara_cb_func_t func;
	unsigned int ret = 0;

	func = udc_func_table.udc_QueryPara;
	if (func != NULL) {
		ret = func(strm, id, param);
	} else {
		ret = -1;//E_NO_EXIST
		pr_notice("exec %s fail: not register!\n", __func__);
	}
	return ret;
}

int register_udc_func_udc_GetCmpLen(udc_GetCmpLen_cb_func_t func)
{
	int ret = 0;

	if (udc_func_table.udc_GetCmpLen == NULL) {
		udc_func_table.udc_GetCmpLen = func;
		pr_notice("%s success\n", __func__);
	} else {
		pr_notice("%s fail: registered!\n", __func__);
		ret = -1;
	}

	return ret;
}

int udc_GetCmpLen_cb(struct z_stream_s *strm,
	unsigned char *start, unsigned char *end)
{
	udc_GetCmpLen_cb_func_t func;
	unsigned int ret = 0;

	func = udc_func_table.udc_GetCmpLen;
	if (func != NULL) {
		ret = func(strm, start, end);
	} else {
		ret = -1;//E_NO_EXIST
		pr_notice("exec %s fail: not register!\n", __func__);
	}
	return ret;
}

int register_udc_functions(unsigned int id, void *f)
{
	switch (id) {
	case ID_deflateInit2:
	{
		deflateInit2_cb_func_t func =
			(deflateInit2_cb_func_t)f;

		return register_udc_func_deflateInit2(func);
	}
	case ID_deflateSetDict:
	{
		deflateSetDict_cb_func_t func =
			(deflateSetDict_cb_func_t)f;

		return register_udc_func_deflateSetDict(func);
	}
	case ID_deflateEnd:
	{
		deflateEnd_cb_func_t func =
			(deflateEnd_cb_func_t)f;

		return register_udc_func_deflateEnd(func);
	}
	case ID_deflateReset:
	{
		deflateReset_cb_func_t func =
			(deflateReset_cb_func_t)f;

		return register_udc_func_deflateReset(func);
	}
	case ID_deflate:
	{
		deflate_cb_func_t func =
			(deflate_cb_func_t)f;

		return register_udc_func_deflate(func);
	}
	case ID_deflateBound:
	{
		deflateBound_cb_func_t func =
			(deflateBound_cb_func_t)f;

		return register_udc_func_deflateBound(func);
	}
	case ID_udc_chksum:
	{
		udc_chksum_cb_func_t func =
			(udc_chksum_cb_func_t)f;

		return register_udc_func_udc_chksum(func);
	}
	case ID_udc_QueryPara:
	{
		udc_QueryPara_cb_func_t func =
			(udc_QueryPara_cb_func_t)f;

		return register_udc_func_udc_QueryPara(func);
	}
	case ID_udc_GetCmpLen:
	{
		udc_GetCmpLen_cb_func_t func =
			(udc_GetCmpLen_cb_func_t)f;

		return register_udc_func_udc_GetCmpLen(func);
	}
	default:
		pr_notice("udc func id %d not found\n", id);
		return -1;
	}
}
EXPORT_SYMBOL(register_udc_functions);

void my_free(void *my_param, void *ptr)
{
	struct udc_private_data *p = (struct udc_private_data *)my_param;

	if (p != NULL && p->mem != NULL && p->used > 0) {
		if ((unsigned char *)ptr >= p->mem &&
			(unsigned char *)ptr < p->mem + p->used) {
			p->used = (unsigned char *)ptr - p->mem;
		}
	}
}

void *my_malloc(void *my_param, unsigned int size, unsigned int count)
{
	struct udc_private_data *p = (struct udc_private_data *)my_param;
	unsigned char *ptr;

	if (p == NULL)
		return NULL;
	if (p->mem == NULL || p->size == 0)
		return NULL;
	if (p->size < p->used + size*count)
		return NULL;
	ptr = p->mem + p->used;
	p->used += size*count;
	return (void *)ptr;
}

int udc_init(struct z_stream_s *zcpr, struct udc_private_data *my_param)
{
	int ret = 0;

	my_param->used = 0;
	ret = udc_QueryPara_cb(NULL, UDC_QUERY_WORKSPACE_SIZE, &my_param->size);
	my_param->mem = vmalloc(my_param->size);

	pr_debug("%s:alloc memory:%p\n",  __func__, my_param->mem);

	(*zcpr).zalloc = &my_malloc;
	(*zcpr).zfree = &my_free;
	(*zcpr).opaque = my_param;

	return ret;
}

void udc_deinit(struct z_stream_s *zcpr)
{
	struct udc_private_data *p =
		(struct udc_private_data *)((*zcpr).opaque);

	pr_debug("%s:free memory:%p\n",  __func__, p->mem);
	vfree(p->mem);
}
