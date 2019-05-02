/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _ION_KERNEL_H
#define _ION_KERNEL_H

#include <linux/dma-buf.h>
#include <linux/bitmap.h>
#include "../uapi/ion.h"
#include "../uapi/msm_ion.h"

#ifdef CONFIG_ION

/*
 * Allocates an ion buffer.
 * Use IS_ERR on returned pointer to check for success.
 */
struct dma_buf *ion_alloc(size_t len, unsigned int heap_id_mask,
			  unsigned int flags);

static inline unsigned int ion_get_flags_num_vm_elems(unsigned int flags)
{
	unsigned long vm_flags = flags & ION_FLAGS_CP_MASK;

	return ((unsigned int)bitmap_weight(&vm_flags, BITS_PER_LONG));
}

int ion_populate_vm_list(unsigned long flags, unsigned int *vm_list,
			 int nelems);

#else

static inline struct dma_buf *ion_alloc(size_t len, unsigned int heap_id_mask,
					unsigned int flags)
{
	return -ENOMEM;
}

static inline unsigned int ion_get_flags_num_vm_elems(unsigned int flags)
{
	return 0;
}

static inline int ion_populate_vm_list(unsigned long flags,
				       unsigned int *vm_list, int nelems)
{
	return -EINVAL;
}

#endif /* CONFIG_ION */
#endif /* _ION_KERNEL_H */
