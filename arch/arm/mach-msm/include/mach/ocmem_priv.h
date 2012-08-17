/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <mach/msm_iomap.h>
#include "ocmem.h"


#define OCMEM_PHYS_BASE 0xFEC00000
#define OCMEM_PHYS_SIZE 0x180000

#define TO_OCMEM 0x0
#define TO_DDR 0x1

struct ocmem_zone;

struct ocmem_zone_ops {
	unsigned long (*allocate) (struct ocmem_zone *, unsigned long);
	int (*free) (struct ocmem_zone *, unsigned long, unsigned long);
};

struct ocmem_zone {
	bool active;
	int owner;
	int active_regions;
	int max_regions;
	struct list_head req_list;
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

/* Operational modes of each region */
enum region_mode {
	WIDE_MODE = 0x0,
	THIN_MODE,
	MODE_DEFAULT = WIDE_MODE,
};

struct ocmem_plat_data {
	void __iomem *vbase;
	unsigned long size;
	unsigned long base;
	struct clk *core_clk;
	struct clk *iface_clk;
	struct ocmem_partition *parts;
	int nr_parts;
	void __iomem *reg_base;
	void __iomem *br_base;
	void __iomem *dm_base;
	unsigned nr_regions;
	unsigned nr_macros;
	unsigned nr_ports;
	int ocmem_irq;
	int dm_irq;
	bool interleaved;
	bool rpm_pwr_ctrl;
	unsigned rpm_rsc_type;
};

struct ocmem_eviction_data {
	struct completion completion;
	struct list_head victim_list;
	struct list_head req_list;
	struct work_struct work;
	int prio;
	int pending;
	bool passive;
};

struct ocmem_req {
	struct rw_semaphore rw_sem;
	/* Chain in sched queue */
	struct list_head sched_list;
	/* Chain in zone list */
	struct list_head zone_list;
	/* Chain in eviction list */
	struct list_head eviction_list;
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
	/* Request Power State */
	unsigned power_state;
	struct ocmem_eviction_data *edata;
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

/* Simple wrappers which will have debug features added later */
static inline int ocmem_read(void *at)
{
	return readl_relaxed(at);
}

static inline int ocmem_write(unsigned long val, void *at)
{
	writel_relaxed(val, at);
	return 0;
}

struct ocmem_zone *get_zone(unsigned);
int zone_active(int);
unsigned long offset_to_phys(unsigned long);
unsigned long phys_to_offset(unsigned long);
unsigned long allocate_head(struct ocmem_zone *, unsigned long);
int free_head(struct ocmem_zone *, unsigned long, unsigned long);
unsigned long allocate_tail(struct ocmem_zone *, unsigned long);
int free_tail(struct ocmem_zone *, unsigned long, unsigned long);

int ocmem_notifier_init(void);
int check_notifier(int);
const char *get_name(int);
int check_id(int);
int dispatch_notification(int, enum ocmem_notif_type, struct ocmem_buf *);

int ocmem_sched_init(void);
int ocmem_rdm_init(struct platform_device *);
int ocmem_core_init(struct platform_device *);
int process_allocate(int, struct ocmem_handle *, unsigned long, unsigned long,
			unsigned long, bool, bool);
int process_free(int, struct ocmem_handle *);
int process_xfer(int, struct ocmem_handle *, struct ocmem_map_list *, int);
int process_evict(int);
int process_restore(int);
int process_shrink(int, struct ocmem_handle *, unsigned long);
int ocmem_rdm_transfer(int, struct ocmem_map_list *,
				unsigned long, int);
unsigned long process_quota(int);
int ocmem_memory_off(int, unsigned long, unsigned long);
int ocmem_memory_on(int, unsigned long, unsigned long);
int ocmem_enable_core_clock(void);
int ocmem_enable_iface_clock(void);
void ocmem_disable_core_clock(void);
void ocmem_disable_iface_clock(void);
#endif
