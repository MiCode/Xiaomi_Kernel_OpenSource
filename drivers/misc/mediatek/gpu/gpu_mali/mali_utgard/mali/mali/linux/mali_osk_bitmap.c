/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2008-2010, 2013-2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_osk_bitmap.c
 * Implementation of the OS abstraction layer for the kernel device driver
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/bitmap.h>
#include <linux/vmalloc.h>
#include "common/mali_kernel_common.h"
#include "mali_osk_types.h"
#include "mali_osk.h"

u32 _mali_osk_bitmap_alloc(struct _mali_osk_bitmap *bitmap)
{
	u32 obj;

	MALI_DEBUG_ASSERT_POINTER(bitmap);

	_mali_osk_spinlock_lock(bitmap->lock);

	obj = find_next_zero_bit(bitmap->table, bitmap->max, bitmap->reserve);

	if (obj < bitmap->max) {
		set_bit(obj, bitmap->table);
	} else {
		obj = -1;
	}

	if (obj != -1)
		--bitmap->avail;
	_mali_osk_spinlock_unlock(bitmap->lock);

	return obj;
}

void _mali_osk_bitmap_free(struct _mali_osk_bitmap *bitmap, u32 obj)
{
	MALI_DEBUG_ASSERT_POINTER(bitmap);

	_mali_osk_bitmap_free_range(bitmap, obj, 1);
}

u32 _mali_osk_bitmap_alloc_range(struct _mali_osk_bitmap *bitmap, int cnt)
{
	u32 obj;

	MALI_DEBUG_ASSERT_POINTER(bitmap);

	if (0 >= cnt) {
		return -1;
	}

	if (1 == cnt) {
		return _mali_osk_bitmap_alloc(bitmap);
	}

	_mali_osk_spinlock_lock(bitmap->lock);
	obj = bitmap_find_next_zero_area(bitmap->table, bitmap->max,
					 bitmap->last, cnt, 0);

	if (obj >= bitmap->max) {
		obj = bitmap_find_next_zero_area(bitmap->table, bitmap->max,
						 bitmap->reserve, cnt, 0);
	}

	if (obj < bitmap->max) {
		bitmap_set(bitmap->table, obj, cnt);

		bitmap->last = (obj + cnt);
		if (bitmap->last >= bitmap->max) {
			bitmap->last = bitmap->reserve;
		}
	} else {
		obj = -1;
	}

	if (obj != -1) {
		bitmap->avail -= cnt;
	}

	_mali_osk_spinlock_unlock(bitmap->lock);

	return obj;
}

u32 _mali_osk_bitmap_avail(struct _mali_osk_bitmap *bitmap)
{
	MALI_DEBUG_ASSERT_POINTER(bitmap);

	return bitmap->avail;
}

void _mali_osk_bitmap_free_range(struct _mali_osk_bitmap *bitmap, u32 obj, int cnt)
{
	MALI_DEBUG_ASSERT_POINTER(bitmap);

	_mali_osk_spinlock_lock(bitmap->lock);
	bitmap_clear(bitmap->table, obj, cnt);
	bitmap->last = min(bitmap->last, obj);

	bitmap->avail += cnt;
	_mali_osk_spinlock_unlock(bitmap->lock);
}

int _mali_osk_bitmap_init(struct _mali_osk_bitmap *bitmap, u32 num, u32 reserve)
{
	MALI_DEBUG_ASSERT_POINTER(bitmap);
	MALI_DEBUG_ASSERT(reserve <= num);

	bitmap->reserve = reserve;
	bitmap->last = reserve;
	bitmap->max  = num;
	bitmap->avail = num - reserve;
	bitmap->lock = _mali_osk_spinlock_init(_MALI_OSK_LOCKFLAG_UNORDERED, _MALI_OSK_LOCK_ORDER_FIRST);
	if (!bitmap->lock) {
		return _MALI_OSK_ERR_NOMEM;
	}
	bitmap->table = kzalloc(BITS_TO_LONGS(bitmap->max) *
				sizeof(long), GFP_KERNEL);
	if (!bitmap->table) {
		_mali_osk_spinlock_term(bitmap->lock);
		return _MALI_OSK_ERR_NOMEM;
	}

	return _MALI_OSK_ERR_OK;
}

void _mali_osk_bitmap_term(struct _mali_osk_bitmap *bitmap)
{
	MALI_DEBUG_ASSERT_POINTER(bitmap);

	if (NULL != bitmap->lock) {
		_mali_osk_spinlock_term(bitmap->lock);
	}

	if (NULL != bitmap->table) {
		kfree(bitmap->table);
	}
}

