/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TMEM_CARVEOUT_HEAP_H_
#define TMEM_CARVEOUT_HEAP_H_

#include <linux/scatterlist.h>

int tmem_carveout_init(void);
int tmem_carveout_create(int idx, phys_addr_t heap_base, size_t heap_size);
int tmem_carveout_destroy(int idx);
int tmem_register_ffa_module(void);
int tmem_run_ffa_test(int test_id);

int tmem_ffa_region_alloc(enum MTEE_MCHUNKS_ID mchunk_id,
		unsigned long size, unsigned long alignment, u64 *ffa_handle);
int tmem_ffa_region_free(enum MTEE_MCHUNKS_ID mchunk_id,
		u64 ffa_handle);

int tmem_ffa_page_alloc(enum MTEE_MCHUNKS_ID mchunk_id,
		struct sg_table *table, u64 *ffa_handle);
int tmem_ffa_page_free(u64 ffa_handle);

#endif /* TMEM_CARVEOUT_HEAP_H_ */
