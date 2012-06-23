/*
 * Copyright (C) 2011 Google, Inc
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/types.h>
#include <mach/scm.h>

#include "ion_cp_common.h"

#define MEM_PROTECT_LOCK_ID	0x05

struct cp2_mem_chunks {
	unsigned int *chunk_list;
	unsigned int chunk_list_size;
	unsigned int chunk_size;
} __attribute__ ((__packed__));

struct cp2_lock_req {
	struct cp2_mem_chunks chunks;
	unsigned int mem_usage;
	unsigned int lock;
} __attribute__ ((__packed__));

int ion_cp_change_chunks_state(unsigned long chunks, unsigned int nchunks,
				unsigned int chunk_size,
				enum cp_mem_usage usage,
				int lock)
{
	struct cp2_lock_req request;

	request.mem_usage = usage;
	request.lock = lock;

	request.chunks.chunk_list = (unsigned int *)chunks;
	request.chunks.chunk_list_size = nchunks;
	request.chunks.chunk_size = chunk_size;

	return scm_call(SCM_SVC_CP, MEM_PROTECT_LOCK_ID,
			&request, sizeof(request), NULL, 0);

}

