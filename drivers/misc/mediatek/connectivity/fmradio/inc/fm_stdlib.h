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

#ifndef __FM_STDLIB_H__
#define __FM_STDLIB_H__

#include "fm_typedef.h"
#include <linux/string.h>
#include <linux/slab.h>

#if 1
#define fm_memset(buf, a, len)  \
({                                    \
	void *__ret = (void *)0;              \
	__ret = memset((buf), (a), (len)); \
	__ret;                          \
})

#define fm_memcpy(dst, src, len)  \
({                                    \
	void *__ret = (void *)0;              \
	__ret = memcpy((dst), (src), (len)); \
	__ret;                          \
})

#define fm_malloc(len)  \
({                                    \
	void *__ret = (void *)0;              \
	__ret = kmalloc(len, GFP_KERNEL); \
	__ret;                          \
})

#define fm_zalloc(len)  \
({                                    \
	void *__ret = (void *)0;              \
	__ret = kzalloc(len, GFP_KERNEL); \
	__ret;                          \
})

#define fm_free(ptr)  kfree(ptr)

#define fm_vmalloc(len)  \
({                                    \
	void *__ret = (void *)0;              \
	__ret = vmalloc(len); \
	__ret;                          \
})

#define fm_vfree(ptr)  vfree(ptr)

#else
inline void *fm_memset(void *buf, signed char val, signed int len)
{
	return memset(buf, val, len);
}

inline void *fm_memcpy(void *dst, const void *src, signed int len)
{
	return memcpy(dst, src, len);
}

#endif

#endif /* __FM_STDLIB_H__ */
