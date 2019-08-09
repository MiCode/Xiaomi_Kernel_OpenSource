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

#ifndef TMEM_MEMORY_LEAK_DETECTION_HELPER_H
#define TMEM_MEMORY_LEAK_DETECTION_HELPER_H

#ifdef TCORE_MEMORY_LEAK_DETECTION_SUPPORT
enum MLD_CHECK_STATUS { MLD_CHECK_PASS = 0, MLD_CHECK_FAIL = 1 };
void mld_init(void);
void *mld_kmalloc(size_t size, gfp_t flags);
void mld_kfree(const void *mem_ptr);
size_t mld_stamp(void);
enum MLD_CHECK_STATUS mld_stamp_check(size_t previous_stamped_size);
#else
#define mld_init()
#define mld_kmalloc(size, flags) kmalloc(size, flags)
#define mld_kfree(mem_ptr) kfree(mem_ptr)
#endif

#endif /* end of TMEM_MEMORY_LEAK_DETECTION_HELPER_H */
