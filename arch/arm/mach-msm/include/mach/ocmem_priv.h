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

#ifndef _ARCH_ARM_MACH_MSM_OCMEM_PRIV_H
#define _ARCH_ARM_MACH_MSM_OCMEM_PRIV_H

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

/* OCMEM Zone specific counters */
/* Must be in sync with zstat_names */
enum ocmem_zstat_item {
	NR_REQUESTS = 0x0,
	NR_SYNC_ALLOCATIONS,
	NR_RANGE_ALLOCATIONS,
	NR_ASYNC_ALLOCATIONS,
	NR_ALLOCATION_FAILS,
	NR_GROWTHS,
	NR_FREES,
	NR_SHRINKS,
	NR_MAPS,
	NR_MAP_FAILS,
	NR_UNMAPS,
	NR_UNMAP_FAILS,
	NR_TRANSFERS_TO_OCMEM,
	NR_TRANSFERS_TO_DDR,
	NR_TRANSFER_FAILS,
	NR_EVICTIONS,
	NR_RESTORES,
	NR_OCMEM_ZSTAT_ITEMS,
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
	atomic_long_t z_stat[NR_OCMEM_ZSTAT_ITEMS];
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
	MODE_NOT_SET = 0x0,
	WIDE_MODE,
	THIN_MODE,
	MODE_DEFAULT = MODE_NOT_SET,
};

struct ocmem_plat_data {
	void __iomem *vbase;
	unsigned long size;
	unsigned long base;
	struct clk *core_clk;
	struct clk *iface_clk;
	struct clk *br_clk;
	struct ocmem_partition *parts;
	int nr_parts;
	void __iomem *reg_base;
	void __iomem *br_base;
	void __iomem *dm_base;
	struct dentry *debug_node;
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

struct ocmem_buf *handle_to_buffer(struct ocmem_handle *);
struct ocmem_handle *buffer_to_handle(struct ocmem_buf *);
struct ocmem_req *handle_to_req(struct ocmem_handle *);
struct ocmem_handle *req_to_handle(struct ocmem_req *);
int ocmem_read(void *);
int ocmem_write(unsigned long, void *);
void inc_ocmem_stat(struct ocmem_zone *, enum ocmem_zstat_item);
unsigned long get_ocmem_stat(struct ocmem_zone *z,
				enum ocmem_zstat_item item);
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

int ocmem_sched_init(struct platform_device *);
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
int ocmem_clear(unsigned long, unsigned long);
unsigned long process_quota(int);
int ocmem_memory_off(int, unsigned long, unsigned long);
int ocmem_memory_on(int, unsigned long, unsigned long);
int ocmem_enable_core_clock(void);
int ocmem_enable_iface_clock(void);
int ocmem_enable_br_clock(void);
void ocmem_disable_core_clock(void);
void ocmem_disable_iface_clock(void);
void ocmem_disable_br_clock(void);
int ocmem_lock(enum ocmem_client, unsigned long, unsigned long,
				enum region_mode);
int ocmem_unlock(enum ocmem_client, unsigned long, unsigned long);
#endif
