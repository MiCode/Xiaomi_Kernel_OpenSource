/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _ION_KERNEL_H
#define _ION_KERNEL_H

#include <linux/dma-buf.h>
#include "../uapi/ion.h"

#ifdef CONFIG_ION

/*
 * Allocates an ion buffer.
 * Use IS_ERR on returned pointer to check for success.
 */
struct dma_buf *ion_alloc(size_t len, unsigned int heap_id_mask,
			  unsigned int flags);

#else

static inline struct dma_buf *ion_alloc(size_t len, unsigned int heap_id_mask,
					unsigned int flags)
{
	return ERR_PTR(-ENOMEM);
}

#endif /* CONFIG_ION */
#endif /* _ION_KERNEL_H */
