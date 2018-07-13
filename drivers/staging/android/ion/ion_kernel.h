/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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
	return -ENOMEM;
}

#endif /* CONFIG_ION */
#endif /* _ION_KERNEL_H */
