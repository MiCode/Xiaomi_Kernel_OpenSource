/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#include <sync.h>
#include "sde_kms.h"
#include "sde_fence.h"

void *sde_sync_get(uint64_t fd)
{
	/* force signed compare, fdget accepts an int argument */
	return (signed int)fd >= 0 ? sync_fence_fdget(fd) : NULL;
}

void sde_sync_put(void *fence)
{
	if (fence)
		sync_fence_put(fence);
}

int sde_sync_wait(void *fence, long timeout_ms)
{
	if (!fence)
		return -EINVAL;
	return sync_fence_wait(fence, timeout_ms);
}

