// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include "sprd_osal.h"

void *sprd_malloc(u32 size)
{
	return kmalloc(size, GFP_KERNEL);
}

void sprd_free(void *addr)
{
	kfree(addr);
}

void sprd_memset(void *addr, u32 value, u32 size)
{
	memset((void *)addr, value, size);
}

void sprd_memset_orig(void *addr, u32 value, u32 size)
{
	memset((void *)addr, value, size);
}

void sprd_memcpy(void *dstaddr, void *srcaddr, u32 size)
{
	memcpy((void *)dstaddr, (void *)srcaddr, size);
}

void sprd_sleep(u32 mstime)
{
}

void *sprd_aligned_malloc(u32 size, u32 align)
{
	return kmalloc(size, GFP_KERNEL);
}


void sprd_aligned_free(void *addr)
{

}
