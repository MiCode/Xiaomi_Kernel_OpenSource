/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _ARCH_ARM_MACH_MSM_OCMEM_CORE_H
#define _ARCH_ARM_MACH_MSM_OCMEM_CORE_H

/** All interfaces in this header should only be used by OCMEM driver
 *  Client drivers should use wrappers available in ocmem.h
 **/

#include "ocmem.h"
#include <mach/msm_iomap.h>
#include <asm/io.h>

#define OCMEM_PHYS_BASE 0xFEC00000
#define OCMEM_PHYS_SIZE 0x180000

#define TO_OCMEM 0x1
#define TO_DDR 0x2

struct ocmem_zone;

struct ocmem_zone_ops {
	unsigned long (*allocate) (struct ocmem_zone *, unsigned long);
	int (*free) (struct ocmem_zone *, unsigned long, unsigned long);
};

struct ocmem_zone {
	int owner;
	int active_regions;
	int max_regions;
	struct list_head region_list;
	unsigned long z_start;
	unsigned long z_end;
	unsigned long z_head;
	unsigned long z_tail;
	unsigned long z_free;
	struct gen_pool *z_pool;
	struct ocmem_zone_ops *z_ops;
};

enum op_code {
	SCHED_NOP = 0x0,
	SCHED_ALLOCATE,
	SCHED_FREE,
	SCHED_GROW,
	SCHED_SHRINK,
	SCHED_MAP,
	SCHED_UNMAP,
	SCHED_EVICT,
	SCHED_RESTORE,
	SCHED_DUMP,
};

struct ocmem_req {
	struct rw_semaphore rw_sem;
	/* Chain in sched queue */
	struct list_head sched_list;
	/* Chain in zone list */
	struct list_head zone_list;
	int owner;
	int prio;
	uint32_t req_id;
	unsigned long req_min;
	unsigned long req_max;
	unsigned long req_step;
	/* reverse pointers */
	struct ocmem_zone *zone;
	struct ocmem_buf *buffer;
	struct ocmem_map_list *mlist;
	enum op_code op;
	unsigned long state;
	/* Request assignments */
	unsigned long req_start;
	unsigned long req_end;
	unsigned long req_sz;
};

struct ocmem_handle {
	struct ocmem_buf buffer;
	struct mutex handle_mutex;
	struct ocmem_req *req;
};

static inline struct ocmem_buf *handle_to_buffer(struct ocmem_handle *handle)
{
	if (handle)
		return &handle->buffer;
	else
		return NULL;
}

static inline struct ocmem_handle *buffer_to_handle(struct ocmem_buf *buffer)
{
	if (buffer)
		return container_of(buffer, struct ocmem_handle, buffer);
	else
		return NULL;
}

static inline struct ocmem_req *handle_to_req(struct ocmem_handle *handle)
{
	if (handle)
		return handle->req;
	else
		return NULL;
}

static inline struct ocmem_handle *req_to_handle(struct ocmem_req *req)
{
	if (req && req->buffer)
		return container_of(req->buffer, struct ocmem_handle, buffer);
	else
		return NULL;
}

struct ocmem_zone *get_zone(unsigned);
unsigned long offset_to_phys(unsigned long);
unsigned long phys_to_offset(unsigned long);
unsigned long allocate_head(struct ocmem_zone *, unsigned long);
int free_head(struct ocmem_zone *, unsigned long, unsigned long);
unsigned long allocate_tail(struct ocmem_zone *, unsigned long);
int free_tail(struct ocmem_zone *, unsigned long, unsigned long);

int ocmem_notifier_init(void);
int check_notifier(int);
int dispatch_notification(int, enum ocmem_notif_type, struct ocmem_buf *);

int ocmem_sched_init(void);
int process_allocate(int, struct ocmem_handle *, unsigned long, unsigned long,
			unsigned long, bool, bool);
int process_free(int, struct ocmem_handle *);
int process_xfer(int, struct ocmem_handle *, struct ocmem_map_list *,
			int direction);
unsigned long process_quota(int);
#endif
