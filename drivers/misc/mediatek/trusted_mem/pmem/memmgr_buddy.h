/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef MEMMGR_BUDDY_H
#define MEMMGR_BUDDY_H

int memmgr_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		 u32 clean);
int memmgr_free(u32 sec_handle);
int memmgr_add_region(u64 pa, u32 size);
int memmgr_remove_region(void);

#endif /* end of MEMMGR_BUDDY_H */
