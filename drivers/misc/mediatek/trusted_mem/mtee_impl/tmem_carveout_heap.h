/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef TMEM_CARVEOUT_HEAP_H_
#define TMEM_CARVEOUT_HEAP_H_

int tmem_carveout_init(void);
int tmem_carveout_create(int idx, phys_addr_t heap_base, size_t heap_size);
int tmem_carveout_destroy(int idx);
int tmem_carveout_heap_alloc(enum MTEE_MCHUNKS_ID mchunk_id,
		unsigned long size, u32 *handle);
int tmem_carveout_heap_free(enum MTEE_MCHUNKS_ID mchunk_id,
		u32 tfa_handle);

#endif /* TMEM_CARVEOUT_HEAP_H_ */
