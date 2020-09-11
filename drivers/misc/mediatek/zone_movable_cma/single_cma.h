/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __SINGLE_CMA_H__
#define __SINGLE_CMA_H__


phys_addr_t zmc_base(void);
struct page *zmc_cma_alloc(struct cma *cma, int count, unsigned int align,
					struct single_cma_registration *p);
bool zmc_cma_release(struct cma *cma, struct page *pages, int count);
#endif
