/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef MEMMGR_BUDDY_H
#define MEMMGR_BUDDY_H

int memmgr_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
		 u32 clean);
int memmgr_free(u32 sec_handle);
int memmgr_add_region(u64 pa, u32 size);
int memmgr_remove_region(void);

#endif /* end of MEMMGR_BUDDY_H */
