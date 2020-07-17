/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2017, 2019-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_POOL_H
#define __KGSL_POOL_H

void kgsl_init_page_pools(struct kgsl_device *device);
void kgsl_exit_page_pools(void);
void kgsl_pool_free_pages(struct page **pages, unsigned int page_count);
int kgsl_pool_alloc_page(int *page_size, struct page **pages,
			unsigned int pages_len, unsigned int *align,
			struct kgsl_memdesc *memdesc);
void kgsl_pool_free_page(struct page *p);
bool kgsl_pool_avaialable(int size);
#endif /* __KGSL_POOL_H */

