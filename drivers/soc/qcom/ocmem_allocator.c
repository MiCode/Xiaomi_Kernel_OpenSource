/* Copyright (c) 2012,2014, The Linux Foundation. All rights reserved.
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

#include <soc/qcom/ocmem.h>
#include "ocmem_priv.h"
#include <linux/genalloc.h>

/* All allocator operations are serialized by ocmem driver */

/* The allocators work as follows:
	Constraints:
	1) There is no IOMMU access to OCMEM hence successive allocations
		in the zone must be physically contiguous
	2) Allocations must be freed in reverse order within a zone.

	z->z_start: Fixed pointer to the start of a zone
	z->z_end:   Fixed pointer to the end of a zone

	z->z_head:  Movable pointer to the next free area when growing at head
			Fixed on zones that grow from tail

	z->z_tail:  Movable pointer to the next free area when growing at tail
			Fixed on zones that grow from head

	z->z_free:  Free space in a zone that is updated on an allocation/free

	reserve:    Enable libgenpool to simulate tail allocations
*/

int allocate_head(struct ocmem_zone *z, unsigned long size,
							unsigned long *offset)
{
	*offset  = gen_pool_alloc(z->z_pool, size);

	if (!(*offset))
		return -ENOMEM;

	z->z_head += size;
	z->z_free -= size;
	return 0;
}

int allocate_tail(struct ocmem_zone *z, unsigned long size,
							unsigned long *offset)
{
	unsigned long reserve;
	unsigned long head;

	if (z->z_tail < (z->z_head + size))
		return -ENOMEM;

	reserve = z->z_tail - z->z_head - size;
	if (reserve) {
		head = gen_pool_alloc(z->z_pool, reserve);
		*offset = gen_pool_alloc(z->z_pool, size);
		gen_pool_free(z->z_pool, head, reserve);
	} else
		*offset = gen_pool_alloc(z->z_pool, size);

	if (!(*offset))
		return -ENOMEM;

	z->z_tail -= size;
	z->z_free -= size;
	return 0;
}

int free_head(struct ocmem_zone *z, unsigned long offset,
			unsigned long size)
{
	if (offset > z->z_head) {
		pr_err("ocmem: Detected out of order free "
				"leading to fragmentation\n");
		return -EINVAL;
	}
	gen_pool_free(z->z_pool, offset, size);
	z->z_head -= size;
	z->z_free += size;
	return 0;
}

int free_tail(struct ocmem_zone *z, unsigned long offset,
				unsigned long size)
{
	if (offset > z->z_tail) {
		pr_err("ocmem: Detected out of order free "
				"leading to fragmentation\n");
		return -EINVAL;
	}
	gen_pool_free(z->z_pool, offset, size);
	z->z_tail += size;
	z->z_free += size;
	return 0;
}
