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
#include <mach/ocmem_priv.h>

enum request_states {
	R_FREE = 0x0,	/* request is not allocated */
	R_PENDING,	/* request has a pending operation */
	R_ALLOCATED,	/* request has been allocated */
	R_MUST_GROW,	/* request must grow as a part of pending operation */
	R_MUST_SHRINK,	/* request must shrink as a part of pending operation */
	R_MUST_MAP,	/* request must be mapped before being used */
	R_MUST_UNMAP,	/* request must be unmapped when not being used */
	R_MAPPED,	/* request is mapped and actively used by client */
	R_UNMAPPED,	/* request is not mapped, so it's not in active use */
	R_EVICTED,	/* request is evicted and must be restored */
};

#define SET_STATE(x, val) (set_bit((val), &(x)->state))
#define CLEAR_STATE(x, val) (clear_bit((val), &(x)->state))
#define TEST_STATE(x, val) (test_bit((val), &(x)->state))

enum op_res {
	OP_COMPLETE = 0x0,
	OP_RESCHED,
	OP_PARTIAL,
	OP_FAIL = ~0x0,
};

/* Represents various client priorities */
/* Note: More than one client can share a priority level */
enum client_prio {
	MIN_PRIO = 0x0,
	NO_PRIO = MIN_PRIO,
	PRIO_SENSORS = 0x1,
	PRIO_BLAST = 0x1,
	PRIO_LP_AUDIO = 0x1,
	PRIO_HP_AUDIO = 0x2,
	PRIO_VOICE = 0x3,
	PRIO_GFX_GROWTH = 0x4,
	PRIO_VIDEO = 0x5,
	PRIO_GFX = 0x6,
	PRIO_OCMEM = 0x7,
	MAX_OCMEM_PRIO = PRIO_OCMEM + 1,
};

static struct list_head sched_queue[MAX_OCMEM_PRIO];
static struct mutex sched_queue_mutex;

/* The duration in msecs before a pending operation is scheduled
 * This allows an idle window between use case boundaries where various
 * hardware state changes can occur. The value will be tweaked on actual
 * hardware.
*/
#define SCHED_DELAY 10

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

/**
 * Primary OCMEM Arbitration Table
 **/
struct ocmem_table {
	int client_id;
	int priority;
	int mode;
	int hw_interconnect;
} ocmem_client_table[OCMEM_CLIENT_MAX] = {
	{OCMEM_GRAPHICS, PRIO_GFX, OCMEM_PERFORMANCE, OCMEM_PORT},
	{OCMEM_VIDEO, PRIO_VIDEO, OCMEM_PERFORMANCE, OCMEM_OCMEMNOC},
	{OCMEM_CAMERA, NO_PRIO, OCMEM_PERFORMANCE, OCMEM_OCMEMNOC},
	{OCMEM_HP_AUDIO, PRIO_HP_AUDIO, OCMEM_PASSIVE, OCMEM_BLOCKED},
	{OCMEM_VOICE, PRIO_VOICE, OCMEM_PASSIVE, OCMEM_BLOCKED},
	{OCMEM_LP_AUDIO, PRIO_LP_AUDIO, OCMEM_LOW_POWER, OCMEM_SYSNOC},
	{OCMEM_SENSORS, PRIO_SENSORS, OCMEM_LOW_POWER, OCMEM_SYSNOC},
	{OCMEM_BLAST, PRIO_BLAST, OCMEM_LOW_POWER, OCMEM_SYSNOC},
};

static struct rb_root sched_tree;
static struct mutex sched_mutex;

/* A region represents a continuous interval in OCMEM address space */
struct ocmem_region {
	/* Chain in Interval Tree */
	struct rb_node region_rb;
	/* Hash map of requests */
	struct idr region_idr;
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

static inline int is_blocked(int id)
{
	return ocmem_client_table[id].hw_interconnect == OCMEM_BLOCKED ? 1 : 0;
}

/* Returns the address that can be used by a device core to access OCMEM */
static unsigned long device_address(int id, unsigned long addr)
{
	int hw_interconnect = ocmem_client_table[id].hw_interconnect;
	unsigned long ret_addr = 0x0;

	switch (hw_interconnect) {
	case OCMEM_PORT:
		ret_addr = phys_to_offset(addr);
		break;
	case OCMEM_OCMEMNOC:
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
		ret_addr = offset_to_phys(addr);
		break;
	case OCMEM_OCMEMNOC:
	case OCMEM_SYSNOC:
		ret_addr = addr;
		break;
	case OCMEM_BLOCKED:
		ret_addr = 0x0;
		break;
	}
	return ret_addr;
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
	p->r_start = p->r_end = p->r_sz = 0x0;
	p->max_prio = NO_PRIO;
	return p;
}

static int destroy_region(struct ocmem_region *region)
{
	kfree(region);
	return 0;
}

static int attach_req(struct ocmem_region *region, struct ocmem_req *req)
{
	int ret, id;

	while (1) {
		if (idr_pre_get(&region->region_idr, GFP_KERNEL) == 0)
			return -ENOMEM;

		ret = idr_get_new_above(&region->region_idr, req, 1, &id);

		if (ret != -EAGAIN)
			break;
	}

	if (!ret) {
		req->req_id = id;
		pr_debug("ocmem: request %p(id:%d) attached to region %p\n",
				req, id, region);
		return 0;
	}
	return -EINVAL;
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

/* Must be called with sched_mutex held */
static int __sched_unmap(struct ocmem_req *req)
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

	mutex_lock(&sched_mutex);
	rc = __sched_map(req);
	mutex_unlock(&sched_mutex);

	if (rc == OP_FAIL)
		return -EINVAL;

	return 0;
}

static int do_unmap(struct ocmem_req *req)
{
	int rc = 0;

	mutex_lock(&sched_mutex);
	rc = __sched_unmap(req);
	mutex_unlock(&sched_mutex);

	if (rc == OP_FAIL)
		return -EINVAL;

	return 0;
}

/* process map is a wrapper where power control will be added later */
static int process_map(struct ocmem_req *req, unsigned long start,
				unsigned long end)
{
	return do_map(req);
}

/* process unmap is a wrapper where power control will be added later */
static int process_unmap(struct ocmem_req *req, unsigned long start,
				unsigned long end)
{
	return do_unmap(req);
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
		alloc_addr = zone->z_ops->allocate(zone, curr_sz + growth_sz);

		if (alloc_addr < 0) {
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

	region = create_region();

	if (!region) {
		pr_err("ocmem: Unable to create region\n");
		goto invalid_op_error;
	}

	retry = false;

	pr_debug("ocmem: ALLOCATE: request size %lx\n", sz);

retry_next_step:

	spanned_r = NULL;
	overlap_r = NULL;

	spanned_r = find_region(zone->z_head);
	overlap_r = find_region_intersection(zone->z_head, zone->z_head + sz);

	if (overlap_r == NULL) {
		/* no conflicting regions, schedule this region */
		alloc_addr = zone->z_ops->allocate(zone, sz);

		if (alloc_addr < 0) {
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
				pr_err("ocmem: Requires eviction support\n");
				goto err_not_supported;
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

static int sched_enqueue(struct ocmem_req *priv)
{
	struct ocmem_req *next = NULL;
	mutex_lock(&sched_queue_mutex);
	list_add_tail(&priv->sched_list, &sched_queue[priv->owner]);
	pr_debug("enqueued req %p\n", priv);
	list_for_each_entry(next, &sched_queue[priv->owner], sched_list) {
		pr_debug("pending requests for client %p\n", next);
	}
	mutex_unlock(&sched_queue_mutex);
	return 0;
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
			break;
			}
		}
	}
	mutex_unlock(&sched_queue_mutex);
	return req;
}

int process_xfer(int id, struct ocmem_handle *handle,
		struct ocmem_map_list *list, int direction)
{

	return 0;
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

	/* Attempt to grow the region */
	rc = do_grow(req);

	if (rc < 0)
		return -EINVAL;

	/* Map the newly grown region */
	if (is_tcm(req->owner)) {
		rc = process_map(req, req->req_start, req->req_end);
		if (rc < 0)
			return -EINVAL;
	}

	/* Notify the client about the buffer growth */
	rc = dispatch_notification(req->owner, OCMEM_ALLOC_GROW, req->buffer);
	if (rc < 0) {
		pr_err("No notifier callback to cater for req %p event: %d\n",
				req, OCMEM_ALLOC_GROW);
		BUG();
	}
	return 0;
}

static void ocmem_sched_wk_func(struct work_struct *work);
DECLARE_DELAYED_WORK(ocmem_sched_thread, ocmem_sched_wk_func);

static int ocmem_schedule_pending(void)
{
	schedule_delayed_work(&ocmem_sched_thread,
				msecs_to_jiffies(SCHED_DELAY));
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
	int rc = 0;

	if (is_blocked(id)) {
		pr_err("Client %d cannot request free\n", id);
		return -EINVAL;
	}

	req = handle_to_req(handle);
	buffer = handle_to_buffer(handle);

	if (!req)
		return -EINVAL;

	if (req->req_start != core_address(id, buffer->addr)) {
		pr_err("Invalid buffer handle passed for free\n");
		return -EINVAL;
	}

	if (is_tcm(req->owner)) {
		rc = process_unmap(req, req->req_start, req->req_end);
		if (rc < 0)
			return -EINVAL;
	}

	rc = do_free(req);

	if (rc < 0)
		return -EINVAL;

	ocmem_destroy_req(req);
	handle->req = NULL;

	ocmem_schedule_pending();
	return 0;
}

static int do_allocate(struct ocmem_req *req, bool can_block, bool can_wait)
{
	int rc = 0;
	struct ocmem_buf *buffer = req->buffer;

	down_write(&req->rw_sem);

	/* Take the scheduler mutex */
	mutex_lock(&sched_mutex);
	rc = __sched_allocate(req, can_block, can_wait);
	mutex_unlock(&sched_mutex);

	if (rc == OP_FAIL)
		goto err_allocate_fail;

	if (rc == OP_RESCHED) {
		buffer->addr = 0x0;
		buffer->len = 0x0;
		pr_debug("ocmem: Enqueuing req %p\n", req);
		sched_enqueue(req);
	} else if (rc == OP_PARTIAL) {
		buffer->addr = device_address(req->owner, req->req_start);
		buffer->len = req->req_sz;
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


int process_allocate(int id, struct ocmem_handle *handle,
			unsigned long min, unsigned long max,
			unsigned long step, bool can_block, bool can_wait)
{

	struct ocmem_req *req = NULL;
	struct ocmem_buf *buffer = NULL;
	int rc = 0;

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

	rc = do_allocate(req, can_block, can_wait);

	if (rc < 0)
		goto do_allocate_error;

	handle->req = req;

	if (is_tcm(id)) {
		rc = process_map(req, req->req_start, req->req_end);
		if (rc < 0)
			goto map_error;
	}

	return 0;

map_error:
	handle->req = NULL;
	do_free(req);
do_allocate_error:
	ocmem_destroy_req(req);
	return -EINVAL;
}

int process_delayed_allocate(struct ocmem_req *req)
{

	struct ocmem_handle *handle = NULL;
	int rc = 0;
	int id = req->owner;

	handle = req_to_handle(req);
	BUG_ON(handle == NULL);

	rc = do_allocate(req, true, false);

	if (rc < 0)
		goto do_allocate_error;

	if (is_tcm(id)) {
		rc = process_map(req, req->req_start, req->req_end);
		if (rc < 0)
			goto map_error;
	}

	/* Notify the client about the buffer growth */
	rc = dispatch_notification(id, OCMEM_ALLOC_GROW, req->buffer);
	if (rc < 0) {
		pr_err("No notifier callback to cater for req %p event: %d\n",
				req, OCMEM_ALLOC_GROW);
		BUG();
	}
	return 0;

map_error:
	handle->req = NULL;
	do_free(req);
do_allocate_error:
	ocmem_destroy_req(req);
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

int ocmem_sched_init(void)
{
	int i = 0;
	sched_tree = RB_ROOT;
	mutex_init(&sched_mutex);
	mutex_init(&sched_queue_mutex);
	for (i = MIN_PRIO; i < MAX_OCMEM_PRIO; i++)
		INIT_LIST_HEAD(&sched_queue[i]);

	return 0;
}
