/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
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
