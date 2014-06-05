/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/rbtree.h>
#include <linux/idr.h>
#include <linux/genalloc.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include "ocmem_priv.h"

enum request_states {
	R_FREE = 0x0,   /* request is not allocated */
	R_PENDING,      /* request has a pending operation */
	R_ALLOCATED,    /* request has been allocated */
	R_ENQUEUED,     /* request has been enqueued for future retry */
	R_MUST_GROW,    /* request must grow as a part of pending operation */
	R_MUST_SHRINK,  /* request must shrink */
	R_WF_SHRINK,    /* shrink must be ack'ed by a client */
	R_SHRUNK,       /* request was shrunk */
	R_MUST_MAP,     /* request must be mapped before being used */
	R_MUST_UNMAP,   /* request must be unmapped when not being used */
	R_MAPPED,       /* request is mapped and actively used by client */
	R_UNMAPPED,     /* request is not mapped, so it's not in active use */
	R_EVICTED,      /* request is evicted and must be restored */
};

#define SET_STATE(x, val) (set_bit((val), &(x)->state))
#define CLEAR_STATE(x, val) (clear_bit((val), &(x)->state))
#define TEST_STATE(x, val) (test_bit((val), &(x)->state))

enum op_res {
	OP_COMPLETE = 0x0,
	OP_RESCHED,
	OP_PARTIAL,
	OP_EVICT,
	OP_FAIL = ~0x0,
};

/* Represents various client priorities */
/* Note: More than one client can share a priority level */
enum client_prio {
	MIN_PRIO = 0x0,
	NO_PRIO = MIN_PRIO,
	PRIO_SENSORS = 0x1,
	PRIO_OTHER_OS = 0x1,
	PRIO_LP_AUDIO = 0x1,
	PRIO_HP_AUDIO = 0x2,
	PRIO_VOICE = 0x3,
	PRIO_GFX_GROWTH = 0x4,
	PRIO_VIDEO = 0x5,
	PRIO_GFX = 0x6,
	PRIO_OCMEM = 0x7,
	MAX_OCMEM_PRIO = PRIO_OCMEM + 1,
};

static void __iomem *ocmem_vaddr;
static struct list_head sched_queue[MAX_OCMEM_PRIO];
static struct mutex sched_queue_mutex;

/* The duration in msecs before a pending operation is scheduled
 * This allows an idle window between use case boundaries where various
 * hardware state changes can occur. The value will be tweaked on actual
 * hardware.
*/
/* Delay in ms for switching to low power mode for OCMEM */
#define SCHED_DELAY 5000

static struct list_head rdm_queue;
static struct mutex rdm_mutex;
static struct workqueue_struct *ocmem_rdm_wq;
static struct workqueue_struct *ocmem_eviction_wq;

static struct ocmem_eviction_data *evictions[OCMEM_CLIENT_MAX];

struct ocmem_rdm_work {
	int id;
	struct ocmem_map_list *list;
	struct ocmem_handle *handle;
	int direction;
	struct work_struct work;
};

/* OCMEM Operational modes */
enum ocmem_client_modes {
	OCMEM_PERFORMANCE = 1,
	OCMEM_PASSIVE,
	OCMEM_LOW_POWER,
	OCMEM_MODE_MAX = OCMEM_LOW_POWER
};

/* OCMEM Addressing modes */
enum ocmem_interconnects {
	OCMEM_BLOCKED = 0,
	OCMEM_PORT = 1,
	OCMEM_OCMEMNOC = 2,
	OCMEM_SYSNOC = 3,
};

enum ocmem_tz_client {
	TZ_UNUSED = 0x0,
	TZ_GRAPHICS,
	TZ_VIDEO,
	TZ_LP_AUDIO,
	TZ_SENSORS,
	TZ_OTHER_OS,
	TZ_DEBUG,
};

/**
 * Primary OCMEM Arbitration Table
 **/
struct ocmem_table {
	int client_id;
	int priority;
	int mode;
	int hw_interconnect;
	int tz_id;
} ocmem_client_table[OCMEM_CLIENT_MAX] = {
	{OCMEM_GRAPHICS, PRIO_GFX, OCMEM_PERFORMANCE, OCMEM_PORT,
								TZ_GRAPHICS},
	{OCMEM_VIDEO, PRIO_VIDEO, OCMEM_PERFORMANCE, OCMEM_OCMEMNOC,
								TZ_VIDEO},
	{OCMEM_CAMERA, NO_PRIO, OCMEM_PERFORMANCE, OCMEM_OCMEMNOC,
								TZ_UNUSED},
	{OCMEM_HP_AUDIO, PRIO_HP_AUDIO, OCMEM_PASSIVE, OCMEM_BLOCKED,
								TZ_UNUSED},
	{OCMEM_VOICE, PRIO_VOICE, OCMEM_PASSIVE, OCMEM_BLOCKED,
								TZ_UNUSED},
	{OCMEM_LP_AUDIO, PRIO_LP_AUDIO, OCMEM_LOW_POWER, OCMEM_SYSNOC,
								TZ_LP_AUDIO},
	{OCMEM_SENSORS, PRIO_SENSORS, OCMEM_LOW_POWER, OCMEM_SYSNOC,
								TZ_SENSORS},
	{OCMEM_OTHER_OS, PRIO_OTHER_OS, OCMEM_LOW_POWER, OCMEM_SYSNOC,
								TZ_OTHER_OS},
};

static struct rb_root sched_tree;
static struct mutex sched_mutex;
static struct mutex allocation_mutex;
static struct mutex free_mutex;

/* A region represents a continuous interval in OCMEM address space */
struct ocmem_region {
	/* Chain in Interval Tree */
	struct rb_node region_rb;
	/* Hash map of requests */
	struct idr region_idr;
	/* Chain in eviction list */
	struct list_head eviction_list;
	unsigned long r_start;
	unsigned long r_end;
	unsigned long r_sz;
	/* Highest priority of all requests served by this region */
	int max_prio;
};

/* Is OCMEM tightly coupled to the client ?*/
static inline int is_tcm(int id)
{
	if (ocmem_client_table[id].hw_interconnect == OCMEM_PORT ||
		ocmem_client_table[id].hw_interconnect == OCMEM_OCMEMNOC)
		return 1;
	else
		return 0;
}

static inline int is_iface_access(int id)
{
	return ocmem_client_table[id].hw_interconnect == OCMEM_OCMEMNOC ? 1 : 0;
}

static inline int is_remapped_access(int id)
{
	return ocmem_client_table[id].hw_interconnect == OCMEM_SYSNOC ? 1 : 0;
}

static inline int is_blocked(int id)
{
	return ocmem_client_table[id].hw_interconnect == OCMEM_BLOCKED ? 1 : 0;
}

inline struct ocmem_buf *handle_to_buffer(struct ocmem_handle *handle)
{
	if (handle)
		return &handle->buffer;
	else
		return NULL;
}

inline struct ocmem_handle *buffer_to_handle(struct ocmem_buf *buffer)
{
	if (buffer)
		return container_of(buffer, struct ocmem_handle, buffer);
	else
		return NULL;
}

inline struct ocmem_req *handle_to_req(struct ocmem_handle *handle)
{
	if (handle)
		return handle->req;
	else
		return NULL;
}

inline struct ocmem_handle *req_to_handle(struct ocmem_req *req)
{
	if (req && req->buffer)
		return container_of(req->buffer, struct ocmem_handle, buffer);
	else
		return NULL;
}

/* Simple wrappers which will have debug features added later */
inline int ocmem_read(void *at)
{
	return readl_relaxed(at);
}

inline int ocmem_write(unsigned long val, void *at)
{
	writel_relaxed(val, at);
	return 0;
}

inline int get_mode(int id)
{
	if (!check_id(id))
		return MODE_DEFAULT;
	else
		return ocmem_client_table[id].mode == OCMEM_PERFORMANCE ?
							WIDE_MODE : THIN_MODE;
}

inline int get_tz_id(int id)
{
	if (!check_id(id))
		return TZ_UNUSED;
	else
		return ocmem_client_table[id].tz_id;
}

/* Returns the address that can be used by a device core to access OCMEM */
static unsigned long device_address(int id, unsigned long addr)
{
	int hw_interconnect = ocmem_client_table[id].hw_interconnect;
	unsigned long ret_addr = 0x0;

	switch (hw_interconnect) {
	case OCMEM_PORT:
	case OCMEM_OCMEMNOC:
		ret_addr = phys_to_offset(addr);
		break;
	case OCMEM_SYSNOC:
		ret_addr = addr;
		break;
	case OCMEM_BLOCKED:
		ret_addr = 0x0;
		break;
	}
	return ret_addr;
}

/* Returns the address as viewed by the core */
static unsigned long core_address(int id, unsigned long addr)
{
	int hw_interconnect = ocmem_client_table[id].hw_interconnect;
	unsigned long ret_addr = 0x0;

	switch (hw_interconnect) {
	case OCMEM_PORT:
	case OCMEM_OCMEMNOC:
		ret_addr = offset_to_phys(addr);
		break;
	case OCMEM_SYSNOC:
		ret_addr = addr;
		break;
	case OCMEM_BLOCKED:
		ret_addr = 0x0;
		break;
	}
	return ret_addr;
}

static inline struct ocmem_zone *zone_of(struct ocmem_req *req)
{
	int owner;
	if (!req)
		return NULL;
	owner = req->owner;
	return get_zone(owner);
}

static int insert_region(struct ocmem_region *region)
{

	struct rb_root *root = &sched_tree;
	struct rb_node **p = &root->rb_node;
	struct rb_node *parent = NULL;
	struct ocmem_region *tmp = NULL;
	unsigned long addr = region->r_start;

	while (*p) {
		parent = *p;
		tmp = rb_entry(parent, struct ocmem_region, region_rb);

		if (tmp->r_end > addr) {
			if (tmp->r_start <= addr)
				break;
			p =  &(*p)->rb_left;
		} else if (tmp->r_end <= addr)
			p = &(*p)->rb_right;
	}
	rb_link_node(&region->region_rb, parent, p);
	rb_insert_color(&region->region_rb, root);
	return 0;
}

static int remove_region(struct ocmem_region *region)
{
	struct rb_root *root = &sched_tree;
	rb_erase(&region->region_rb, root);
	return 0;
}

static struct ocmem_req *ocmem_create_req(void)
{
	struct ocmem_req *p = NULL;

	p =  kzalloc(sizeof(struct ocmem_req), GFP_KERNEL);
	if (!p)
		return NULL;

	INIT_LIST_HEAD(&p->zone_list);
	INIT_LIST_HEAD(&p->sched_list);
	init_rwsem(&p->rw_sem);
	SET_STATE(p, R_FREE);
	pr_debug("request %p created\n", p);
	return p;
}

static int ocmem_destroy_req(struct ocmem_req *req)
{
	kfree(req);
	return 0;
}

static struct ocmem_region *create_region(void)
{
	struct ocmem_region *p = NULL;

	p =  kzalloc(sizeof(struct ocmem_region), GFP_KERNEL);
	if (!p)
		return NULL;
	idr_init(&p->region_idr);
	INIT_LIST_HEAD(&p->eviction_list);
	p->r_start = p->r_end = p->r_sz = 0x0;
	p->max_prio = NO_PRIO;
	return p;
}

static int destroy_region(struct ocmem_region *region)
{
	idr_destroy(&region->region_idr);
	kfree(region);
	return 0;
}

static int attach_req(struct ocmem_region *region, struct ocmem_req *req)
{
	int id;

	id = idr_alloc(&region->region_idr, req, 1, 0, GFP_KERNEL);
	if (id < 0)
		return id;

	req->req_id = id;
	pr_debug("ocmem: request %p(id:%d) attached to region %p\n",
			req, id, region);
	return 0;
}

static int detach_req(struct ocmem_region *region, struct ocmem_req *req)
{
	idr_remove(&region->region_idr, req->req_id);
	return 0;
}

static int populate_region(struct ocmem_region *region, struct ocmem_req *req)
{
	region->r_start = req->req_start;
	region->r_end = req->req_end;
	region->r_sz =  req->req_end - req->req_start + 1;
	return 0;
}

static int region_req_count(int id, void *ptr, void *data)
{
	int *count = data;
	*count = *count + 1;
	return 0;
}

static int req_count(struct ocmem_region *region)
{
	int count = 0;
	idr_for_each(&region->region_idr, region_req_count, &count);
	return count;
}

static int compute_max_prio(int id, void *ptr, void *data)
{
	int *max = data;
	struct ocmem_req *req = ptr;

	if (req->prio > *max)
		*max = req->prio;
	return 0;
}

static int update_region_prio(struct ocmem_region *region)
{
	int max_prio;
	if (req_count(region) != 0) {
		idr_for_each(&region->region_idr, compute_max_prio, &max_prio);
		region->max_prio = max_prio;
	} else {
		region->max_prio = NO_PRIO;
	}
	pr_debug("ocmem: Updating prio of region %p as %d\n",
			region, max_prio);

	return 0;
}

static struct ocmem_region *find_region(unsigned long addr)
{
	struct ocmem_region *region = NULL;
	struct rb_node *rb_node = NULL;

	rb_node = sched_tree.rb_node;

	while (rb_node) {
		struct ocmem_region *tmp_region = NULL;
		tmp_region = rb_entry(rb_node, struct ocmem_region, region_rb);

		if (tmp_region->r_end > addr) {
			region = tmp_region;
			if (tmp_region->r_start <= addr)
				break;
			rb_node = rb_node->rb_left;
		} else {
			rb_node = rb_node->rb_right;
		}
	}
	return region;
}

static struct ocmem_region *find_region_intersection(unsigned long start,
					unsigned long end)
{

	struct ocmem_region *region = NULL;
	region = find_region(start);
	if (region && end <= region->r_start)
		region = NULL;
	return region;
}

static struct ocmem_region *find_region_match(unsigned long start,
					unsigned long end)
{

	struct ocmem_region *region = NULL;
	region = find_region(start);
	if (region && start == region->r_start && end == region->r_end)
		return region;
	return NULL;
}

static struct ocmem_req *find_req_match(int owner, struct ocmem_region *region)
{
	struct ocmem_req *req = NULL;

	if (!region)
		return NULL;

	req = idr_find(&region->region_idr, owner);

	return req;
}

/* Must be called with req->sem held */
static inline int is_mapped(struct ocmem_req *req)
{
	return TEST_STATE(req, R_MAPPED);
}

static inline int is_pending_shrink(struct ocmem_req *req)
{
	return TEST_STATE(req, R_MUST_SHRINK) ||
		TEST_STATE(req, R_WF_SHRINK);
}

/* Must be called with sched_mutex held */
static int __sched_unmap(struct ocmem_req *req)
{
	struct ocmem_req *matched_req = NULL;
	struct ocmem_region *matched_region = NULL;

	if (!TEST_STATE(req, R_MAPPED))
		goto invalid_op_error;

	matched_region = find_region_match(req->req_start, req->req_end);
	matched_req = find_req_match(req->req_id, matched_region);

	if (!matched_region || !matched_req) {
		pr_err("Could not find backing region for req");
		goto invalid_op_error;
	}

	if (matched_req != req) {
		pr_err("Request does not match backing req");
		goto invalid_op_error;
	}

	if (!is_mapped(req)) {
		pr_err("Request is not currently mapped");
		goto invalid_op_error;
	}

	/* Update the request state */
	CLEAR_STATE(req, R_MAPPED);
	SET_STATE(req, R_MUST_MAP);

	return OP_COMPLETE;

invalid_op_error:
	return OP_FAIL;
}

/* Must be called with sched_mutex held */
static int __sched_map(struct ocmem_req *req)
{
	struct ocmem_req *matched_req = NULL;
	struct ocmem_region *matched_region = NULL;

	matched_region = find_region_match(req->req_start, req->req_end);
	matched_req = find_req_match(req->req_id, matched_region);

	if (!matched_region || !matched_req) {
		pr_err("Could not find backing region for req");
		goto invalid_op_error;
	}

	if (matched_req != req) {
		pr_err("Request does not match backing req");
		goto invalid_op_error;
	}

	/* Update the request state */
	CLEAR_STATE(req, R_MUST_MAP);
	SET_STATE(req, R_MAPPED);

	return OP_COMPLETE;

invalid_op_error:
	return OP_FAIL;
}

static int do_map(struct ocmem_req *req)
{
	int rc = 0;

	down_write(&req->rw_sem);

	mutex_lock(&sched_mutex);
	rc = __sched_map(req);
	mutex_unlock(&sched_mutex);

	up_write(&req->rw_sem);

	if (rc == OP_FAIL)
		return -EINVAL;

	return 0;
}

static int do_unmap(struct ocmem_req *req)
{
	int rc = 0;

	down_write(&req->rw_sem);

	mutex_lock(&sched_mutex);
	rc = __sched_unmap(req);
	mutex_unlock(&sched_mutex);

	up_write(&req->rw_sem);

	if (rc == OP_FAIL)
		return -EINVAL;

	return 0;
}

static int process_map(struct ocmem_req *req, unsigned long start,
				unsigned long end)
{
	int rc = 0;

	rc = ocmem_restore_sec_program(OCMEM_SECURE_DEV_ID);

	if (rc < 0) {
		pr_err("ocmem: Failed to restore security programming\n");
		goto lock_failed;
	}

	rc = ocmem_lock(req->owner, phys_to_offset(req->req_start), req->req_sz,
							get_mode(req->owner));

	if (rc < 0) {
		pr_err("ocmem: Failed to secure request %p for %d\n", req,
				req->owner);
		goto lock_failed;
	}

	rc = do_map(req);

	if (rc < 0) {
		pr_err("ocmem: Failed to map request %p for %d\n",
							req, req->owner);
		goto process_map_fail;

	}
	pr_debug("ocmem: Mapped request %p\n", req);
	return 0;

process_map_fail:
	ocmem_unlock(req->owner, phys_to_offset(req->req_start), req->req_sz);
lock_failed:
	pr_err("ocmem: Failed to map ocmem request\n");
	return rc;
}

static int process_unmap(struct ocmem_req *req, unsigned long start,
				unsigned long end)
{
	int rc = 0;

	rc = do_unmap(req);

	if (rc < 0)
		goto process_unmap_fail;

	rc = ocmem_unlock(req->owner, phys_to_offset(req->req_start),
						req->req_sz);

	if (rc < 0) {
		pr_err("ocmem: Failed to un-secure request %p for %d\n", req,
				req->owner);
		goto unlock_failed;
	}

	pr_debug("ocmem: Unmapped request %p\n", req);
	return 0;

unlock_failed:
process_unmap_fail:
	pr_err("ocmem: Failed to unmap ocmem request\n");
	return rc;
}

static int __sched_grow(struct ocmem_req *req, bool can_block)
{
	unsigned long min = req->req_min;
	unsigned long max = req->req_max;
	unsigned long step = req->req_step;
	int owner = req->owner;
	unsigned long curr_sz = 0;
	unsigned long growth_sz = 0;
	unsigned long curr_start = 0;
	enum client_prio prio = req->prio;
	unsigned long alloc_addr = 0x0;
	bool retry;
	struct ocmem_region *spanned_r = NULL;
	struct ocmem_region *overlap_r = NULL;
	int rc = 0;

	struct ocmem_req *matched_req = NULL;
	struct ocmem_region *matched_region = NULL;

	struct ocmem_zone *zone = get_zone(owner);
	struct ocmem_region *region = NULL;

	matched_region = find_region_match(req->req_start, req->req_end);
	matched_req = find_req_match(req->req_id, matched_region);

	if (!matched_region || !matched_req) {
		pr_err("Could not find backing region for req");
		goto invalid_op_error;
	}

	if (matched_req != req) {
		pr_err("Request does not match backing req");
		goto invalid_op_error;
	}

	curr_sz = matched_req->req_sz;
	curr_start = matched_req->req_start;
	growth_sz = matched_req->req_max - matched_req->req_sz;

	pr_debug("Attempting to grow req %p from %lx to %lx\n",
			req, matched_req->req_sz, matched_req->req_max);

	retry = false;

	pr_debug("ocmem: GROW: growth size %lx\n", growth_sz);

retry_next_step:

	spanned_r = NULL;
	overlap_r = NULL;

	spanned_r = find_region(zone->z_head);
	overlap_r = find_region_intersection(zone->z_head,
				zone->z_head + growth_sz);

	if (overlap_r == NULL) {
		/* no conflicting regions, schedule this region */
		zone->z_ops->free(zone, curr_start, curr_sz);
		rc = zone->z_ops->allocate(zone, curr_sz + growth_sz,
								&alloc_addr);

		if (rc) {
			pr_err("ocmem: zone allocation operation failed\n");
			goto internal_error;
		}

		curr_sz += growth_sz;
		/* Detach the region from the interval tree */
		/* This is to guarantee that any change in size
		 * causes the tree to be rebalanced if required */

		detach_req(matched_region, req);
		if (req_count(matched_region) == 0) {
			remove_region(matched_region);
			region = matched_region;
		} else {
			region = create_region();
			if (!region) {
				pr_err("ocmem: Unable to create region\n");
				goto region_error;
			}
		}

		/* update the request */
		req->req_start = alloc_addr;
		/* increment the size to reflect new length */
		req->req_sz = curr_sz;
		req->req_end = alloc_addr + req->req_sz - 1;

		/* update request state */
		CLEAR_STATE(req, R_MUST_GROW);
		SET_STATE(req, R_ALLOCATED);
		SET_STATE(req, R_MUST_MAP);
		req->op = SCHED_MAP;

		/* update the region with new req */
		attach_req(region, req);
		populate_region(region, req);
		update_region_prio(region);

		/* update the tree with new region */
		if (insert_region(region)) {
			pr_err("ocmem: Failed to insert the region\n");
			goto region_error;
		}

		if (retry) {
			SET_STATE(req, R_MUST_GROW);
			SET_STATE(req, R_PENDING);
			req->op = SCHED_GROW;
			return OP_PARTIAL;
		}
	} else if (spanned_r != NULL && overlap_r != NULL) {
		/* resolve conflicting regions based on priority */
		if (overlap_r->max_prio < prio) {
			/* Growth cannot be triggered unless a previous
			 * client of lower priority was evicted */
			pr_err("ocmem: Invalid growth scheduled\n");
			/* This is serious enough to fail */
			BUG();
			return OP_FAIL;
		} else if (overlap_r->max_prio > prio) {
			if (min == max) {
				/* Cannot grow at this time, try later */
				SET_STATE(req, R_PENDING);
				SET_STATE(req, R_MUST_GROW);
				return OP_RESCHED;
			} else {
			/* Try to grow in steps */
				growth_sz -= step;
				/* We are OOM at this point so need to retry */
				if (growth_sz <= curr_sz) {
					SET_STATE(req, R_PENDING);
					SET_STATE(req, R_MUST_GROW);
					return OP_RESCHED;
				}
				retry = true;
				pr_debug("ocmem: Attempting with reduced size %lx\n",
						growth_sz);
				goto retry_next_step;
			}
		} else {
			pr_err("ocmem: grow: New Region %p Existing %p\n",
				matched_region, overlap_r);
			pr_err("ocmem: Undetermined behavior\n");
			/* This is serious enough to fail */
			BUG();
		}
	} else if (spanned_r == NULL && overlap_r != NULL) {
		goto err_not_supported;
	}

	return OP_COMPLETE;

err_not_supported:
	pr_err("ocmem: Scheduled unsupported operation\n");
	return OP_FAIL;
region_error:
	zone->z_ops->free(zone, alloc_addr, curr_sz);
	detach_req(region, req);
	update_region_prio(region);
	/* req is going to be destroyed by the caller anyways */
internal_error:
	destroy_region(region);
invalid_op_error:
	return OP_FAIL;
}

/* Must be called with sched_mutex held */
static int __sched_free(struct ocmem_req *req)
{
	int owner = req->owner;
	int ret = 0;

	struct ocmem_req *matched_req = NULL;
	struct ocmem_region *matched_region = NULL;

	struct ocmem_zone *zone = get_zone(owner);

	BUG_ON(!zone);

	matched_region = find_region_match(req->req_start, req->req_end);
	matched_req = find_req_match(req->req_id, matched_region);

	if (!matched_region || !matched_req)
		goto invalid_op_error;
	if (matched_req != req)
		goto invalid_op_error;

	ret = zone->z_ops->free(zone,
		matched_req->req_start, matched_req->req_sz);

	if (ret < 0)
		goto err_op_fail;

	detach_req(matched_region, matched_req);
	update_region_prio(matched_region);
	if (req_count(matched_region) == 0) {
		remove_region(matched_region);
		destroy_region(matched_region);
	}

	/* Update the request */
	req->req_start = 0x0;
	req->req_sz = 0x0;
	req->req_end = 0x0;
	SET_STATE(req, R_FREE);
	return OP_COMPLETE;
invalid_op_error:
	pr_err("ocmem: free: Failed to find matching region\n");
err_op_fail:
	pr_err("ocmem: free: Failed\n");
	return OP_FAIL;
}

/* Must be called with sched_mutex held */
static int __sched_shrink(struct ocmem_req *req, unsigned long new_sz)
{
	int owner = req->owner;
	int ret = 0;

	struct ocmem_req *matched_req = NULL;
	struct ocmem_region *matched_region = NULL;
	struct ocmem_region *region = NULL;
	unsigned long alloc_addr = 0x0;
	int rc =  0;

	struct ocmem_zone *zone = get_zone(owner);

	BUG_ON(!zone);

	/* The shrink should not be called for zero size */
	BUG_ON(new_sz == 0);

	matched_region = find_region_match(req->req_start, req->req_end);
	matched_req = find_req_match(req->req_id, matched_region);

	if (!matched_region || !matched_req)
		goto invalid_op_error;
	if (matched_req != req)
		goto invalid_op_error;

	ret = zone->z_ops->free(zone,
		matched_req->req_start, matched_req->req_sz);

	if (ret < 0) {
		pr_err("Zone Allocation operation failed\n");
		goto internal_error;
	}

	rc = zone->z_ops->allocate(zone, new_sz, &alloc_addr);

	if (rc) {
		pr_err("Zone Allocation operation failed\n");
		goto internal_error;
	}

	/* Detach the region from the interval tree */
	/* This is to guarantee that the change in size
	 * causes the tree to be rebalanced if required */

	detach_req(matched_region, req);
	if (req_count(matched_region) == 0) {
		remove_region(matched_region);
		region = matched_region;
	} else {
		region = create_region();
		if (!region) {
			pr_err("ocmem: Unable to create region\n");
			goto internal_error;
		}
	}
	/* update the request */
	req->req_start = alloc_addr;
	req->req_sz = new_sz;
	req->req_end = alloc_addr + req->req_sz;

	if (req_count(region) == 0) {
		remove_region(matched_region);
		destroy_region(matched_region);
	}

	/* update request state */
	SET_STATE(req, R_MUST_GROW);
	SET_STATE(req, R_MUST_MAP);
	req->op = SCHED_MAP;

	/* attach the request to the region */
	attach_req(region, req);
	populate_region(region, req);
	update_region_prio(region);

	/* update the tree with new region */
	if (insert_region(region)) {
		pr_err("ocmem: Failed to insert the region\n");
		zone->z_ops->free(zone, alloc_addr, new_sz);
		detach_req(region, req);
		update_region_prio(region);
		/* req will be destroyed by the caller */
		goto region_error;
	}
	return OP_COMPLETE;

region_error:
	destroy_region(region);
internal_error:
	pr_err("ocmem: shrink: Failed\n");
	return OP_FAIL;
invalid_op_error:
	pr_err("ocmem: shrink: Failed to find matching region\n");
	return OP_FAIL;
}

/* Must be called with sched_mutex held */
static int __sched_allocate(struct ocmem_req *req, bool can_block,
				bool can_wait)
{
	unsigned long min = req->req_min;
	unsigned long max = req->req_max;
	unsigned long step = req->req_step;
	int owner = req->owner;
	unsigned long sz = max;
	enum client_prio prio = req->prio;
	unsigned long alloc_addr = 0x0;
	bool retry;
	int rc = 0;

	struct ocmem_region *spanned_r = NULL;
	struct ocmem_region *overlap_r = NULL;

	struct ocmem_zone *zone = get_zone(owner);
	struct ocmem_region *region = NULL;

	BUG_ON(!zone);

	if (min > (zone->z_end - zone->z_start)) {
		pr_err("ocmem: requested minimum size exceeds quota\n");
		goto invalid_op_error;
	}

	if (max > (zone->z_end - zone->z_start)) {
		pr_err("ocmem: requested maximum size exceeds quota\n");
		goto invalid_op_error;
	}

	if (min > zone->z_free) {
			pr_err("ocmem: out of memory for zone %d\n", owner);
			goto invalid_op_error;
	}

	retry = false;

	pr_debug("ocmem: do_allocate: %s request %p size %lx\n",
						get_name(owner), req, sz);

retry_next_step:

	spanned_r = NULL;
	overlap_r = NULL;

	spanned_r = find_region(zone->z_head);
	overlap_r = find_region_intersection(zone->z_head, zone->z_head + sz);

	if (overlap_r == NULL) {

		region = create_region();

		if (!region) {
			pr_err("ocmem: Unable to create region\n");
			goto invalid_op_error;
		}

		/* no conflicting regions, schedule this region */
		rc = zone->z_ops->allocate(zone, sz, &alloc_addr);

		if (rc) {
			pr_err("Zone Allocation operation failed\n");
			goto internal_error;
		}

		/* update the request */
		req->req_start = alloc_addr;
		req->req_end = alloc_addr + sz - 1;
		req->req_sz = sz;
		req->zone = zone;

		/* update request state */
		CLEAR_STATE(req, R_FREE);
		CLEAR_STATE(req, R_PENDING);
		SET_STATE(req, R_ALLOCATED);
		SET_STATE(req, R_MUST_MAP);
		req->op = SCHED_NOP;

		/* attach the request to the region */
		attach_req(region, req);
		populate_region(region, req);
		update_region_prio(region);

		/* update the tree with new region */
		if (insert_region(region)) {
			pr_err("ocmem: Failed to insert the region\n");
			zone->z_ops->free(zone, alloc_addr, sz);
			detach_req(region, req);
			update_region_prio(region);
			/* req will be destroyed by the caller */
			goto internal_error;
		}

		if (retry) {
			SET_STATE(req, R_MUST_GROW);
			SET_STATE(req, R_PENDING);
			req->op = SCHED_GROW;
			return OP_PARTIAL;
		}
	} else if (spanned_r != NULL && overlap_r != NULL) {
		/* resolve conflicting regions based on priority */
		if (overlap_r->max_prio < prio) {
			if (min == max) {
				req->req_start = zone->z_head;
				req->req_end = zone->z_head + sz - 1;
				req->req_sz = 0x0;
				req->edata = NULL;
				goto trigger_eviction;
			} else {
			/* Try to allocate atleast >= 'min' immediately */
				sz -= step;
				if (sz < min)
					goto err_out_of_mem;
				retry = true;
				pr_debug("ocmem: Attempting with reduced size %lx\n",
						sz);
				goto retry_next_step;
			}
		} else if (overlap_r->max_prio > prio) {
			if (can_block == true) {
				SET_STATE(req, R_PENDING);
				SET_STATE(req, R_MUST_GROW);
				return OP_RESCHED;
			} else {
				if (min == max) {
					pr_err("Cannot allocate %lx synchronously\n",
							sz);
					goto err_out_of_mem;
				} else {
					sz -= step;
					if (sz < min)
						goto err_out_of_mem;
					retry = true;
					pr_debug("ocmem: Attempting reduced size %lx\n",
							sz);
					goto retry_next_step;
				}
			}
		} else {
			pr_err("ocmem: Undetermined behavior\n");
			pr_err("ocmem: New Region %p Existing %p\n", region,
					overlap_r);
			/* This is serious enough to fail */
			BUG();
		}
	} else if (spanned_r == NULL && overlap_r != NULL)
		goto err_not_supported;

	return OP_COMPLETE;

trigger_eviction:
	pr_debug("Trigger eviction of region %p\n", overlap_r);
	return OP_EVICT;

err_not_supported:
	pr_err("ocmem: Scheduled unsupported operation\n");
	return OP_FAIL;

err_out_of_mem:
	pr_err("ocmem: Out of memory during allocation\n");
internal_error:
	destroy_region(region);
invalid_op_error:
	return OP_FAIL;
}

/* Remove the request from eviction lists */
static void cancel_restore(struct ocmem_req *req)
{
	struct ocmem_eviction_data *edata;

	if (!req)
		return;

	edata = req->eviction_info;

	if (!edata)
		return;

	if (list_empty(&edata->req_list))
		return;

	list_del_init(&req->eviction_list);
	req->eviction_info = NULL;

	return;
}

static int sched_enqueue(struct ocmem_req *priv)
{
	struct ocmem_req *next = NULL;
	mutex_lock(&sched_queue_mutex);
	SET_STATE(priv, R_ENQUEUED);
	list_add_tail(&priv->sched_list, &sched_queue[priv->owner]);
	pr_debug("enqueued req %p\n", priv);
	list_for_each_entry(next, &sched_queue[priv->owner], sched_list) {
		pr_debug("pending request %p for client %s\n", next,
				get_name(next->owner));
	}
	mutex_unlock(&sched_queue_mutex);
	return 0;
}

static void sched_dequeue(struct ocmem_req *victim_req)
{
	struct ocmem_req *req = NULL;
	struct ocmem_req *next = NULL;
	int id;

	if (!victim_req)
		return;

	id = victim_req->owner;

	mutex_lock(&sched_queue_mutex);

	if (list_empty(&sched_queue[id]))
		goto dequeue_done;

	list_for_each_entry_safe(req, next, &sched_queue[id], sched_list)
	{
		if (req == victim_req) {
			pr_debug("ocmem: Cancelling pending request %p for %s\n",
						req, get_name(req->owner));
			list_del_init(&victim_req->sched_list);
			CLEAR_STATE(victim_req, R_ENQUEUED);
			break;
		}
	}
dequeue_done:
	mutex_unlock(&sched_queue_mutex);
	return;
}

static struct ocmem_req *ocmem_fetch_req(void)
{
	int i;
	struct ocmem_req *req = NULL;
	struct ocmem_req *next = NULL;

	mutex_lock(&sched_queue_mutex);
	for (i = MIN_PRIO; i < MAX_OCMEM_PRIO; i++) {
		if (list_empty(&sched_queue[i]))
			continue;
		list_for_each_entry_safe(req, next, &sched_queue[i], sched_list)
		{
			if (req) {
				pr_debug("ocmem: Fetched pending request %p\n",
									req);
				list_del(&req->sched_list);
				CLEAR_STATE(req, R_ENQUEUED);
				break;
			}
		}
	}
	mutex_unlock(&sched_queue_mutex);
	return req;
}


unsigned long process_quota(int id)
{
	struct ocmem_zone *zone = NULL;

	if (is_blocked(id))
		return 0;

	zone = get_zone(id);

	if (zone && zone->z_pool)
		return zone->z_end - zone->z_start;
	else
		return 0;
}

static int do_grow(struct ocmem_req *req)
{
	struct ocmem_buf *buffer = NULL;
	bool can_block = true;
	int rc = 0;

	down_write(&req->rw_sem);
	buffer = req->buffer;

	/* Take the scheduler mutex */
	mutex_lock(&sched_mutex);
	rc = __sched_grow(req, can_block);
	mutex_unlock(&sched_mutex);

	if (rc == OP_FAIL)
		goto err_op_fail;

	if (rc == OP_RESCHED) {
		pr_debug("ocmem: Enqueue this allocation");
		sched_enqueue(req);
	}

	else if (rc == OP_COMPLETE || rc == OP_PARTIAL) {
		buffer->addr = device_address(req->owner, req->req_start);
		buffer->len = req->req_sz;
	}

	up_write(&req->rw_sem);
	return 0;
err_op_fail:
	up_write(&req->rw_sem);
	return -EINVAL;
}

static int process_grow(struct ocmem_req *req)
{
	int rc = 0;
	unsigned long offset = 0;

	/* Attempt to grow the region */
	rc = do_grow(req);

	if (rc < 0)
		return -EINVAL;

	rc = ocmem_enable_core_clock();

	if (rc < 0)
		goto core_clock_fail;

	if (is_iface_access(req->owner)) {
		rc = ocmem_enable_iface_clock();

		if (rc < 0)
			goto iface_clock_fail;
	}

	rc = process_map(req, req->req_start, req->req_end);
	if (rc < 0)
		goto map_error;

	offset = phys_to_offset(req->req_start);

	rc = ocmem_memory_on(req->owner, offset, req->req_sz);

	if (rc < 0) {
		pr_err("Failed to switch ON memory macros\n");
		goto power_ctl_error;
	}

	/* Notify the client about the buffer growth */
	rc = dispatch_notification(req->owner, OCMEM_ALLOC_GROW, req->buffer);
	if (rc < 0) {
		pr_err("No notifier callback to cater for req %p event: %d\n",
				req, OCMEM_ALLOC_GROW);
		BUG();
	}
	return 0;

power_ctl_error:
map_error:
if (is_iface_access(req->owner))
	ocmem_disable_iface_clock();
iface_clock_fail:
	ocmem_disable_core_clock();
core_clock_fail:
	return -EINVAL;
}

static int do_shrink(struct ocmem_req *req, unsigned long shrink_size)
{

	int rc = 0;
	struct ocmem_buf *buffer = NULL;

	down_write(&req->rw_sem);
	buffer = req->buffer;

	/* Take the scheduler mutex */
	mutex_lock(&sched_mutex);
	rc = __sched_shrink(req, shrink_size);
	mutex_unlock(&sched_mutex);

	if (rc == OP_FAIL)
		goto err_op_fail;

	else if (rc == OP_COMPLETE) {
		buffer->addr = device_address(req->owner, req->req_start);
		buffer->len = req->req_sz;
	}

	up_write(&req->rw_sem);
	return 0;
err_op_fail:
	up_write(&req->rw_sem);
	return -EINVAL;
}

static void ocmem_sched_wk_func(struct work_struct *work);
DECLARE_DELAYED_WORK(ocmem_sched_thread, ocmem_sched_wk_func);

static int ocmem_schedule_pending(void)
{

	bool need_sched = false;
	int i = 0;

	for (i = MIN_PRIO; i < MAX_OCMEM_PRIO; i++) {
		if (!list_empty(&sched_queue[i])) {
			need_sched = true;
			break;
		}
	}

	if (need_sched == true) {
		cancel_delayed_work(&ocmem_sched_thread);
		schedule_delayed_work(&ocmem_sched_thread,
					msecs_to_jiffies(SCHED_DELAY));
		pr_debug("ocmem: Scheduled delayed work\n");
	}
	return 0;
}

static int do_free(struct ocmem_req *req)
{
	int rc = 0;
	struct ocmem_buf *buffer = req->buffer;

	down_write(&req->rw_sem);

	if (is_mapped(req)) {
		pr_err("ocmem: Buffer needs to be unmapped before free\n");
		goto err_free_fail;
	}

	pr_debug("ocmem: do_free: client %s req %p\n", get_name(req->owner),
					req);
	/* Grab the sched mutex */
	mutex_lock(&sched_mutex);
	rc = __sched_free(req);
	mutex_unlock(&sched_mutex);

	switch (rc) {

	case OP_COMPLETE:
		buffer->addr = 0x0;
		buffer->len = 0x0;
		break;
	case OP_FAIL:
	default:
		goto err_free_fail;
		break;
	}

	up_write(&req->rw_sem);
	return 0;
err_free_fail:
	up_write(&req->rw_sem);
	pr_err("ocmem: freeing req %p failed\n", req);
	return -EINVAL;
}

int process_free(int id, struct ocmem_handle *handle)
{
	struct ocmem_req *req = NULL;
	struct ocmem_buf *buffer = NULL;
	unsigned long offset = 0;
	int rc = 0;

	mutex_lock(&free_mutex);

	if (is_blocked(id)) {
		pr_err("Client %d cannot request free\n", id);
		goto free_invalid;
	}

	req = handle_to_req(handle);
	buffer = handle_to_buffer(handle);

	if (!req) {
		pr_err("ocmem: No valid request to free\n");
		goto free_invalid;
	}

	if (req->req_start != core_address(id, buffer->addr)) {
		pr_err("Invalid buffer handle passed for free\n");
		goto free_invalid;
	}

	if (req->edata != NULL) {
		pr_err("ocmem: Request %p(%2lx) yet to process eviction %p\n",
					req, req->state, req->edata);
		goto free_invalid;
	}

	if (is_pending_shrink(req)) {
		pr_err("ocmem: Request %p(%2lx) yet to process eviction\n",
					req, req->state);
		goto pending_shrink;
	}

	/* Remove the request from any restore lists */
	if (req->eviction_info)
		cancel_restore(req);

	/* Remove the request from any pending opreations */
	if (TEST_STATE(req, R_ENQUEUED)) {
		mutex_lock(&sched_mutex);
		sched_dequeue(req);
		mutex_unlock(&sched_mutex);
	}

	if (!TEST_STATE(req, R_FREE)) {

		if (TEST_STATE(req, R_MAPPED)) {
			/* unmap the interval and clear the memory */
			rc = process_unmap(req, req->req_start, req->req_end);

			if (rc < 0) {
				pr_err("ocmem: Failed to unmap %p\n", req);
				goto free_fail;
			}
			/* Turn off the memory */
			if (req->req_sz != 0) {

				offset = phys_to_offset(req->req_start);
				rc = ocmem_memory_off(req->owner, offset,
					req->req_sz);

				if (rc < 0) {
					pr_err("Failed to switch OFF memory macros\n");
					goto free_fail;
				}
			}

			if (is_iface_access(req->owner))
				ocmem_disable_iface_clock();
			ocmem_disable_core_clock();

			rc = do_free(req);
			if (rc < 0) {
				pr_err("ocmem: Failed to free %p\n", req);
				goto free_fail;
			}
		} else
			pr_debug("request %p was already shrunk to 0\n", req);
	}

	if (!TEST_STATE(req, R_FREE)) {
		/* Turn off the memory */
		if (req->req_sz != 0) {

			offset = phys_to_offset(req->req_start);
			rc = ocmem_memory_off(req->owner, offset, req->req_sz);

			if (rc < 0) {
				pr_err("Failed to switch OFF memory macros\n");
				goto free_fail;
			}
			if (is_iface_access(req->owner))
				ocmem_disable_iface_clock();
			ocmem_disable_core_clock();
		}

		/* free the allocation */
		rc = do_free(req);
		if (rc < 0)
			return -EINVAL;
	}

	inc_ocmem_stat(zone_of(req), NR_FREES);

	ocmem_destroy_req(req);
	handle->req = NULL;

	ocmem_schedule_pending();
	mutex_unlock(&free_mutex);
	return 0;
free_fail:
free_invalid:
	mutex_unlock(&free_mutex);
	return -EINVAL;
pending_shrink:
	mutex_unlock(&free_mutex);
	return -EAGAIN;
}

static void ocmem_rdm_worker(struct work_struct *work)
{
	int offset = 0;
	int rc = 0;
	int event;
	struct ocmem_rdm_work *work_data = container_of(work,
				struct ocmem_rdm_work, work);
	int id = work_data->id;
	struct ocmem_map_list *list = work_data->list;
	int direction = work_data->direction;
	struct ocmem_handle *handle = work_data->handle;
	struct ocmem_req *req = handle_to_req(handle);
	struct ocmem_buf *buffer = handle_to_buffer(handle);

	down_write(&req->rw_sem);
	offset = phys_to_offset(req->req_start);
	rc = ocmem_rdm_transfer(id, list, offset, direction);
	if (work_data->direction == TO_OCMEM)
		event = (rc == 0) ? OCMEM_MAP_DONE : OCMEM_MAP_FAIL;
	else
		event = (rc == 0) ? OCMEM_UNMAP_DONE : OCMEM_UNMAP_FAIL;
	up_write(&req->rw_sem);
	kfree(work_data);
	dispatch_notification(id, event, buffer);
}

int queue_transfer(struct ocmem_req *req, struct ocmem_handle *handle,
			struct ocmem_map_list *list, int direction)
{
	struct ocmem_rdm_work *work_data = NULL;

	down_write(&req->rw_sem);

	work_data = kzalloc(sizeof(struct ocmem_rdm_work), GFP_ATOMIC);
	if (!work_data)
		BUG();

	work_data->handle = handle;
	work_data->list = list;
	work_data->id = req->owner;
	work_data->direction = direction;
	INIT_WORK(&work_data->work, ocmem_rdm_worker);
	up_write(&req->rw_sem);
	queue_work(ocmem_rdm_wq, &work_data->work);
	return 0;
}

int process_drop(int id, struct ocmem_handle *handle,
				 struct ocmem_map_list *list)
{
	struct ocmem_req *req = NULL;
	struct ocmem_buf *buffer = NULL;
	int rc = 0;

	if (is_blocked(id)) {
		pr_err("Client %d cannot request drop\n", id);
		return -EINVAL;
	}

	if (is_tcm(id))
		pr_err("Client %d cannot request drop\n", id);

	req = handle_to_req(handle);
	buffer = handle_to_buffer(handle);

	if (!req)
		return -EINVAL;

	if (req->req_start != core_address(id, buffer->addr)) {
		pr_err("Invalid buffer handle passed for drop\n");
		return -EINVAL;
	}

	if (TEST_STATE(req, R_MAPPED)) {
		rc = process_unmap(req, req->req_start, req->req_end);
		if (rc < 0)
			return -EINVAL;

		if (is_iface_access(req->owner))
			ocmem_disable_iface_clock();
		ocmem_disable_core_clock();
	} else
		return -EINVAL;

	return 0;
}

int process_xfer_out(int id, struct ocmem_handle *handle,
			struct ocmem_map_list *list)
{
	struct ocmem_req *req = NULL;
	int rc = 0;

	req = handle_to_req(handle);

	if (!req)
		return -EINVAL;

	if (!is_mapped(req)) {
		pr_err("Buffer is not currently mapped\n");
		goto transfer_out_error;
	}

	rc = queue_transfer(req, handle, list, TO_DDR);

	if (rc < 0) {
		pr_err("Failed to queue rdm transfer to DDR\n");
		inc_ocmem_stat(zone_of(req), NR_TRANSFER_FAILS);
		goto transfer_out_error;
	}

	inc_ocmem_stat(zone_of(req), NR_TRANSFERS_TO_DDR);
	return 0;

transfer_out_error:
	return -EINVAL;
}

int process_xfer_in(int id, struct ocmem_handle *handle,
			struct ocmem_map_list *list)
{
	struct ocmem_req *req = NULL;
	int rc = 0;

	req = handle_to_req(handle);

	if (!req)
		return -EINVAL;


	if (!is_mapped(req)) {
		pr_err("Buffer is not already mapped for transfer\n");
		goto transfer_in_error;
	}

	inc_ocmem_stat(zone_of(req), NR_TRANSFERS_TO_OCMEM);
	rc = queue_transfer(req, handle, list, TO_OCMEM);

	if (rc < 0) {
		pr_err("Failed to queue rdm transfer to OCMEM\n");
		inc_ocmem_stat(zone_of(req), NR_TRANSFER_FAILS);
		goto transfer_in_error;
	}

	return 0;
transfer_in_error:
	return -EINVAL;
}

int process_shrink(int id, struct ocmem_handle *handle, unsigned long size)
{
	struct ocmem_req *req = NULL;
	struct ocmem_buf *buffer = NULL;
	struct ocmem_eviction_data *edata = NULL;
	int rc = 0;

	if (is_blocked(id)) {
		pr_err("Client %d cannot request free\n", id);
		return -EINVAL;
	}

	req = handle_to_req(handle);
	buffer = handle_to_buffer(handle);

	if (!req)
		return -EINVAL;

	mutex_lock(&free_mutex);

	if (req->req_start != core_address(id, buffer->addr)) {
		pr_err("Invalid buffer handle passed for shrink\n");
		goto shrink_fail;
	}

	edata = req->eviction_info;

	if (!edata) {
		pr_err("Unable to find eviction data\n");
		goto shrink_fail;
	}

	pr_debug("Found edata %p in request %p\n", edata, req);

	inc_ocmem_stat(zone_of(req), NR_SHRINKS);

	if (size == 0) {
		pr_debug("req %p being shrunk to zero\n", req);
		if (is_mapped(req)) {
			rc = process_unmap(req, req->req_start, req->req_end);
			if (rc < 0)
				goto shrink_fail;
			if (is_iface_access(req->owner))
				ocmem_disable_iface_clock();
			ocmem_disable_core_clock();
		}
		rc = do_free(req);
		if (rc < 0)
			goto shrink_fail;
		SET_STATE(req, R_FREE);
	} else {
		rc = do_shrink(req, size);
		if (rc < 0)
			goto shrink_fail;
	}

	CLEAR_STATE(req, R_ALLOCATED);
	CLEAR_STATE(req, R_WF_SHRINK);
	SET_STATE(req, R_SHRUNK);

	if (atomic_dec_and_test(&edata->pending)) {
		pr_debug("ocmem: All conflicting allocations were shrunk\n");
		complete(&edata->completion);
	}

	mutex_unlock(&free_mutex);
	return 0;
shrink_fail:
	pr_err("ocmem: Failed to shrink request %p of %s\n",
			req, get_name(req->owner));
	mutex_unlock(&free_mutex);
	return -EINVAL;
}

int process_xfer(int id, struct ocmem_handle *handle,
		struct ocmem_map_list *list, int direction)
{
	int rc = 0;

	if (is_tcm(id)) {
		WARN(1, "Mapping operation is invalid for client\n");
		return -EINVAL;
	}

	if (direction == TO_DDR)
		rc = process_xfer_out(id, handle, list);
	else if (direction == TO_OCMEM)
		rc = process_xfer_in(id, handle, list);
	return rc;
}

static struct ocmem_eviction_data *init_eviction(int id)
{
	struct ocmem_eviction_data *edata = NULL;
	int prio = ocmem_client_table[id].priority;

	edata = kzalloc(sizeof(struct ocmem_eviction_data), GFP_ATOMIC);

	if (!edata) {
		pr_err("ocmem: Could not allocate eviction data\n");
		return NULL;
	}

	INIT_LIST_HEAD(&edata->victim_list);
	INIT_LIST_HEAD(&edata->req_list);
	edata->prio = prio;
	atomic_set(&edata->pending, 0);
	return edata;
}

static void free_eviction(struct ocmem_eviction_data *edata)
{

	if (!edata)
		return;

	if (!list_empty(&edata->req_list))
		pr_err("ocmem: Eviction data %p not empty\n", edata);

	kfree(edata);
	edata = NULL;
}

static bool is_overlapping(struct ocmem_req *new, struct ocmem_req *old)
{

	if (!new || !old)
		return false;

	pr_debug("check overlap [%lx -- %lx] on [%lx -- %lx]\n",
			new->req_start, new->req_end,
			old->req_start, old->req_end);

	if ((new->req_start < old->req_start &&
		new->req_end >= old->req_start) ||
		(new->req_start >= old->req_start &&
		 new->req_start <= old->req_end &&
		 new->req_end >= old->req_end)) {
		pr_debug("request %p overlaps with existing req %p\n",
						new, old);
		return true;
	}
	return false;
}

static int __evict_common(struct ocmem_eviction_data *edata,
						struct ocmem_req *req)
{
	struct rb_node *rb_node = NULL;
	struct ocmem_req *e_req = NULL;
	bool needs_eviction = false;
	int j = 0;

	for (rb_node = rb_first(&sched_tree); rb_node;
			rb_node = rb_next(rb_node)) {

		struct ocmem_region *tmp_region = NULL;

		tmp_region = rb_entry(rb_node, struct ocmem_region, region_rb);

		if (tmp_region->max_prio < edata->prio) {
			for (j = edata->prio - 1; j > NO_PRIO; j--) {
				needs_eviction = false;
				e_req = find_req_match(j, tmp_region);
				if (!e_req)
					continue;
				if (edata->passive == true) {
					needs_eviction = true;
				} else {
					needs_eviction = is_overlapping(req,
								e_req);
				}

				if (needs_eviction) {
					pr_debug("adding %p in region %p to eviction list\n",
							e_req, tmp_region);
					SET_STATE(e_req, R_MUST_SHRINK);
					list_add_tail(
						&e_req->eviction_list,
						&edata->req_list);
					atomic_inc(&edata->pending);
					e_req->eviction_info = edata;
				}
			}
		} else {
			pr_debug("Skipped region %p\n", tmp_region);
		}
	}

	pr_debug("%d requests will be evicted\n", atomic_read(&edata->pending));

	return atomic_read(&edata->pending);
}

static void trigger_eviction(struct ocmem_eviction_data *edata)
{
	struct ocmem_req *req = NULL;
	struct ocmem_req *next = NULL;
	struct ocmem_buf buffer;

	if (!edata)
		return;

	BUG_ON(atomic_read(&edata->pending) == 0);

	init_completion(&edata->completion);

	list_for_each_entry_safe(req, next, &edata->req_list, eviction_list)
	{
		if (req) {
			pr_debug("ocmem: Evicting request %p\n", req);
			buffer.addr = req->req_start;
			buffer.len = 0x0;
			CLEAR_STATE(req, R_MUST_SHRINK);
			dispatch_notification(req->owner, OCMEM_ALLOC_SHRINK,
								&buffer);
			SET_STATE(req, R_WF_SHRINK);
		}
	}
	return;
}

int process_evict(int id)
{
	struct ocmem_eviction_data *edata = NULL;
	int rc = 0;

	edata = init_eviction(id);

	if (!edata)
		return -EINVAL;

	edata->passive = true;

	mutex_lock(&sched_mutex);

	rc = __evict_common(edata, NULL);

	if (rc == 0)
		goto skip_eviction;

	trigger_eviction(edata);

	evictions[id] = edata;

	mutex_unlock(&sched_mutex);

	wait_for_completion(&edata->completion);

	return 0;

skip_eviction:
	evictions[id] = NULL;
	mutex_unlock(&sched_mutex);
	return 0;
}

static int run_evict(struct ocmem_req *req)
{
	struct ocmem_eviction_data *edata = NULL;
	int rc = 0;

	if (!req)
		return -EINVAL;

	edata = init_eviction(req->owner);

	if (!edata)
		return -EINVAL;

	edata->passive = false;

	mutex_lock(&free_mutex);
	rc = __evict_common(edata, req);

	if (rc == 0)
		goto skip_eviction;

	trigger_eviction(edata);

	pr_debug("ocmem: attaching eviction %p to request %p", edata, req);
	req->edata = edata;

	mutex_unlock(&free_mutex);

	wait_for_completion(&edata->completion);

	pr_debug("ocmem: eviction completed successfully\n");
	return 0;

skip_eviction:
	pr_err("ocmem: Unable to run eviction\n");
	free_eviction(edata);
	req->edata = NULL;
	mutex_unlock(&free_mutex);
	return 0;
}

static int __restore_common(struct ocmem_eviction_data *edata)
{

	struct ocmem_req *req = NULL;

	if (!edata)
		return -EINVAL;

	while (!list_empty(&edata->req_list)) {
		req = list_first_entry(&edata->req_list, struct ocmem_req,
						eviction_list);
		list_del_init(&req->eviction_list);
		pr_debug("ocmem: restoring evicted request %p\n",
							req);
		req->edata = NULL;
		req->eviction_info = NULL;
		req->op = SCHED_ALLOCATE;
		inc_ocmem_stat(zone_of(req), NR_RESTORES);
		sched_enqueue(req);
	}

	pr_debug("Scheduled all evicted regions\n");

	return 0;
}

static int sched_restore(struct ocmem_req *req)
{

	int rc = 0;

	if (!req)
		return -EINVAL;

	if (!req->edata)
		return 0;

	mutex_lock(&free_mutex);
	rc = __restore_common(req->edata);
	mutex_unlock(&free_mutex);

	if (rc < 0)
		return -EINVAL;

	free_eviction(req->edata);
	req->edata = NULL;
	return 0;
}

int process_restore(int id)
{
	struct ocmem_eviction_data *edata = evictions[id];
	int rc = 0;

	if (!edata) {
		pr_err("Client %s invoked restore without any eviction\n",
					get_name(id));
		return -EINVAL;
	}

	mutex_lock(&free_mutex);
	rc = __restore_common(edata);
	mutex_unlock(&free_mutex);

	if (rc < 0) {
		pr_err("Failed to restore evicted requests\n");
		return -EINVAL;
	}

	free_eviction(edata);
	evictions[id] = NULL;
	ocmem_schedule_pending();
	return 0;
}

static int do_allocate(struct ocmem_req *req, bool can_block, bool can_wait)
{
	int rc = 0;
	int ret = 0;
	struct ocmem_buf *buffer = req->buffer;

	down_write(&req->rw_sem);

retry_allocate:

	/* Take the scheduler mutex */
	mutex_lock(&sched_mutex);
	rc = __sched_allocate(req, can_block, can_wait);
	mutex_unlock(&sched_mutex);

	if (rc == OP_EVICT) {

		mutex_lock(&allocation_mutex);
		ret = run_evict(req);

		if (ret == 0) {
			rc = sched_restore(req);
			if (rc < 0) {
				pr_err("Failed to restore for req %p\n", req);
				mutex_unlock(&allocation_mutex);
				goto err_allocate_fail;
			}
			req->edata = NULL;

			pr_debug("Attempting to re-allocate req %p\n", req);
			req->req_start = 0x0;
			req->req_end = 0x0;
			mutex_unlock(&allocation_mutex);
			goto retry_allocate;
		} else {
			mutex_unlock(&allocation_mutex);
			goto err_allocate_fail;
		}
	}

	if (rc == OP_FAIL) {
		inc_ocmem_stat(zone_of(req), NR_ALLOCATION_FAILS);
		goto err_allocate_fail;
	}

	if (rc == OP_RESCHED) {
		buffer->addr = 0x0;
		buffer->len = 0x0;
		pr_debug("ocmem: Enqueuing req %p\n", req);
		sched_enqueue(req);
	} else if (rc == OP_PARTIAL) {
		buffer->addr = device_address(req->owner, req->req_start);
		buffer->len = req->req_sz;
		inc_ocmem_stat(zone_of(req), NR_RANGE_ALLOCATIONS);
		pr_debug("ocmem: Enqueuing req %p\n", req);
		sched_enqueue(req);
	} else if (rc == OP_COMPLETE) {
		buffer->addr = device_address(req->owner, req->req_start);
		buffer->len = req->req_sz;
	}

	up_write(&req->rw_sem);
	return 0;
err_allocate_fail:
	up_write(&req->rw_sem);
	return -EINVAL;
}

static int do_dump(struct ocmem_req *req, unsigned long addr)
{

	void __iomem *req_vaddr;
	unsigned long offset = 0x0;
	int rc = 0;

	down_write(&req->rw_sem);

	offset = phys_to_offset(req->req_start);

	req_vaddr = ocmem_vaddr + offset;

	if (!req_vaddr)
		goto err_do_dump;

	rc = ocmem_enable_dump(req->owner, offset, req->req_sz);

	if (rc < 0)
		goto err_do_dump;

	pr_debug("Dumping client %s buffer ocmem p: %lx (v: %p) to ddr %lx\n",
				get_name(req->owner), req->req_start,
				req_vaddr, addr);

	memcpy((void *)addr, req_vaddr, req->req_sz);

	rc = ocmem_disable_dump(req->owner, offset, req->req_sz);

	if (rc < 0)
		pr_err("Failed to secure request %p of %s after dump\n",
				req, get_name(req->owner));

	up_write(&req->rw_sem);
	return 0;
err_do_dump:
	up_write(&req->rw_sem);
	return -EINVAL;
}

int process_allocate(int id, struct ocmem_handle *handle,
			unsigned long min, unsigned long max,
			unsigned long step, bool can_block, bool can_wait)
{

	struct ocmem_req *req = NULL;
	struct ocmem_buf *buffer = NULL;
	int rc = 0;
	unsigned long offset = 0;

	/* sanity checks */
	if (is_blocked(id)) {
		pr_err("Client %d cannot request allocation\n", id);
		return -EINVAL;
	}

	if (handle->req != NULL) {
		pr_err("Invalid handle passed in\n");
		return -EINVAL;
	}

	buffer = handle_to_buffer(handle);
	BUG_ON(buffer == NULL);

	/* prepare a request structure to represent this transaction */
	req = ocmem_create_req();
	if (!req)
		return -ENOMEM;

	req->owner = id;
	req->req_min = min;
	req->req_max = max;
	req->req_step = step;
	req->prio = ocmem_client_table[id].priority;
	req->op = SCHED_ALLOCATE;
	req->buffer = buffer;

	inc_ocmem_stat(zone_of(req), NR_REQUESTS);

	rc = do_allocate(req, can_block, can_wait);

	if (rc < 0)
		goto do_allocate_error;

	inc_ocmem_stat(zone_of(req), NR_SYNC_ALLOCATIONS);

	handle->req = req;

	if (req->req_sz != 0) {

		rc = ocmem_enable_core_clock();

		if (rc < 0)
			goto core_clock_fail;

		if (is_iface_access(req->owner)) {
			rc = ocmem_enable_iface_clock();

			if (rc < 0)
				goto iface_clock_fail;
		}
		rc = process_map(req, req->req_start, req->req_end);
		if (rc < 0)
			goto map_error;

		offset = phys_to_offset(req->req_start);

		rc = ocmem_memory_on(req->owner, offset, req->req_sz);

		if (rc < 0) {
			pr_err("Failed to switch ON memory macros\n");
			goto power_ctl_error;
		}
	}

	return 0;

power_ctl_error:
	process_unmap(req, req->req_start, req->req_end);
map_error:
	handle->req = NULL;
	do_free(req);
	if (is_iface_access(req->owner))
		ocmem_disable_iface_clock();
iface_clock_fail:
	ocmem_disable_core_clock();
core_clock_fail:
do_allocate_error:
	ocmem_destroy_req(req);
	return -EINVAL;
}

int process_delayed_allocate(struct ocmem_req *req)
{

	struct ocmem_handle *handle = NULL;
	int rc = 0;
	int id = req->owner;
	unsigned long offset = 0;

	handle = req_to_handle(req);
	BUG_ON(handle == NULL);

	rc = do_allocate(req, true, false);

	if (rc < 0)
		goto do_allocate_error;

	/* The request can still be pending */
	if (TEST_STATE(req, R_PENDING))
		return 0;

	inc_ocmem_stat(zone_of(req), NR_ASYNC_ALLOCATIONS);

	if (req->req_sz != 0) {
		rc = ocmem_enable_core_clock();

		if (rc < 0)
			goto core_clock_fail;

		if (is_iface_access(req->owner)) {
			rc = ocmem_enable_iface_clock();

			if (rc < 0)
				goto iface_clock_fail;
		}

		rc = process_map(req, req->req_start, req->req_end);
		if (rc < 0)
			goto map_error;


		offset = phys_to_offset(req->req_start);

		rc = ocmem_memory_on(req->owner, offset, req->req_sz);

		if (rc < 0) {
			pr_err("Failed to switch ON memory macros\n");
			goto power_ctl_error;
		}
	}

	/* Notify the client about the buffer growth */
	rc = dispatch_notification(id, OCMEM_ALLOC_GROW, req->buffer);
	if (rc < 0) {
		pr_err("No notifier callback to cater for req %p event: %d\n",
				req, OCMEM_ALLOC_GROW);
		BUG();
	}
	return 0;

power_ctl_error:
	process_unmap(req, req->req_start, req->req_end);
map_error:
	handle->req = NULL;
	do_free(req);
	if (is_iface_access(req->owner))
		ocmem_disable_iface_clock();
iface_clock_fail:
	ocmem_disable_core_clock();
core_clock_fail:
do_allocate_error:
	ocmem_destroy_req(req);
	return -EINVAL;
}

int process_dump(int id, struct ocmem_handle *handle, unsigned long addr)
{
	struct ocmem_req *req = NULL;
	int rc = 0;

	req = handle_to_req(handle);

	if (!req)
		return -EINVAL;

	if (!is_mapped(req)) {
		pr_err("Buffer is not mapped\n");
		goto dump_error;
	}

	inc_ocmem_stat(zone_of(req), NR_DUMP_REQUESTS);

	mutex_lock(&sched_mutex);
	rc = do_dump(req, addr);
	mutex_unlock(&sched_mutex);

	if (rc < 0)
		goto dump_error;

	inc_ocmem_stat(zone_of(req), NR_DUMP_COMPLETE);
	return 0;

dump_error:
	pr_err("Dumping OCMEM memory failed for client %d\n", id);
	return -EINVAL;
}

static void ocmem_sched_wk_func(struct work_struct *work)
{

	struct ocmem_buf *buffer = NULL;
	struct ocmem_handle *handle = NULL;
	struct ocmem_req *req = ocmem_fetch_req();

	if (!req) {
		pr_debug("No Pending Requests found\n");
		return;
	}

	pr_debug("ocmem: sched_wk pending req %p\n", req);
	handle = req_to_handle(req);
	buffer = handle_to_buffer(handle);
	BUG_ON(req->op == SCHED_NOP);

	switch (req->op) {
	case SCHED_GROW:
		process_grow(req);
		break;
	case SCHED_ALLOCATE:
		process_delayed_allocate(req);
		break;
	default:
		pr_err("ocmem: Unknown operation encountered\n");
		break;
	}
	return;
}

static int ocmem_allocations_show(struct seq_file *f, void *dummy)
{
	struct rb_node *rb_node = NULL;
	struct ocmem_req *req = NULL;
	unsigned j;
	mutex_lock(&sched_mutex);
	for (rb_node = rb_first(&sched_tree); rb_node;
				rb_node = rb_next(rb_node)) {
		struct ocmem_region *tmp_region = NULL;
		tmp_region = rb_entry(rb_node, struct ocmem_region, region_rb);
		for (j = MAX_OCMEM_PRIO - 1; j > NO_PRIO; j--) {
			req = find_req_match(j, tmp_region);
			if (req) {
				seq_printf(f,
					"owner: %s 0x%lx -- 0x%lx size 0x%lx [state: %2lx]\n",
					get_name(req->owner),
					req->req_start, req->req_end,
					req->req_sz, req->state);
			}
		}
	}
	mutex_unlock(&sched_mutex);
	return 0;
}

static int ocmem_allocations_open(struct inode *inode, struct file *file)
{
	return single_open(file, ocmem_allocations_show, inode->i_private);
}

static const struct file_operations allocations_show_fops = {
	.open = ocmem_allocations_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int ocmem_sched_init(struct platform_device *pdev)
{
	int i = 0;
	struct ocmem_plat_data *pdata = NULL;
	struct device   *dev = &pdev->dev;

	sched_tree = RB_ROOT;
	pdata = platform_get_drvdata(pdev);
	mutex_init(&allocation_mutex);
	mutex_init(&free_mutex);
	mutex_init(&sched_mutex);
	mutex_init(&sched_queue_mutex);
	ocmem_vaddr = pdata->vbase;
	for (i = MIN_PRIO; i < MAX_OCMEM_PRIO; i++)
		INIT_LIST_HEAD(&sched_queue[i]);

	mutex_init(&rdm_mutex);
	INIT_LIST_HEAD(&rdm_queue);
	ocmem_rdm_wq = alloc_workqueue("ocmem_rdm_wq", 0, 0);
	if (!ocmem_rdm_wq)
		return -ENOMEM;
	ocmem_eviction_wq = alloc_workqueue("ocmem_eviction_wq", 0, 0);
	if (!ocmem_eviction_wq)
		return -ENOMEM;

	if (!debugfs_create_file("allocations", S_IRUGO, pdata->debug_node,
					NULL, &allocations_show_fops)) {
		dev_err(dev, "Unable to create debugfs node for scheduler\n");
		return -EBUSY;
	}
	return 0;
}
